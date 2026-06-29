#include "session_manager.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "platform_display.h"
#include "platform_power.h"
#include "platform_touch.h"
#include "storage_manager.h"

static const char *TAG = "session";

#define SESSION_POLL_MS                       100
#define SESSION_DEFAULT_IDLE_TIMEOUT_MS      15000
#define SESSION_DEFAULT_DIM_TIMEOUT_MS       30000
#define SESSION_DEFAULT_SLEEP_TIMEOUT_MS     45000
#define SESSION_DEFAULT_DEEP_SLEEP_TIMEOUT_MS 180000
#define SESSION_DEFAULT_ACTIVE_BRIGHTNESS    100
#define SESSION_DEFAULT_DIM_BRIGHTNESS       15
#define SESSION_DEEP_SLEEP_RETRY_MS          5000

#define SESSION_KEY_IDLE_TIMEOUT_MS          "sess_idle_ms"
#define SESSION_KEY_DIM_TIMEOUT_MS           "sess_dim_ms"
#define SESSION_KEY_SLEEP_TIMEOUT_MS         "sess_sleep_ms"
#define SESSION_KEY_DEEP_SLEEP_TIMEOUT_MS    "sess_ds_ms"
#define SESSION_KEY_ACTIVE_BRIGHTNESS        "sess_active_br"
#define SESSION_KEY_DIM_BRIGHTNESS           "sess_dim_br"

static SemaphoreHandle_t state_lock;
static session_status_t session = {
    .state = SESSION_STATE_ACTIVE,
    .reason = SESSION_REASON_BOOT,
    .brightness = SESSION_DEFAULT_ACTIVE_BRIGHTNESS,
    .idle_ms = 0,
};
static int64_t last_activity_us;
static int64_t deep_sleep_retry_after_us;
static bool external_power_present;
static bool power_status_known;
static session_config_t session_config = {
    .idle_timeout_ms = SESSION_DEFAULT_IDLE_TIMEOUT_MS,
    .dim_timeout_ms = SESSION_DEFAULT_DIM_TIMEOUT_MS,
    .sleep_timeout_ms = SESSION_DEFAULT_SLEEP_TIMEOUT_MS,
    .deep_sleep_timeout_ms = SESSION_DEFAULT_DEEP_SLEEP_TIMEOUT_MS,
    .active_brightness = SESSION_DEFAULT_ACTIVE_BRIGHTNESS,
    .dim_brightness = SESSION_DEFAULT_DIM_BRIGHTNESS,
};

static const char *session_state_name(session_state_t state)
{
    switch (state) {
    case SESSION_STATE_ACTIVE:
        return "ACTIVE";
    case SESSION_STATE_IDLE:
        return "IDLE";
    case SESSION_STATE_DIM:
        return "DIM";
    case SESSION_STATE_SLEEP_REQUESTED:
        return "SLEEP";
    default:
        return "?";
    }
}

static const char *session_reason_name(session_reason_t reason)
{
    switch (reason) {
    case SESSION_REASON_BOOT:
        return "BOOT";
    case SESSION_REASON_INPUT:
        return "INPUT";
    case SESSION_REASON_TIMEOUT:
        return "TIMEOUT";
    case SESSION_REASON_POWER_EVENT:
        return "POWER_EVENT";
    case SESSION_REASON_POWER_BUTTON:
        return "POWER_BUTTON";
    default:
        return "?";
    }
}

static bool session_config_is_valid(const session_config_t *config)
{
    return config != NULL &&
           config->idle_timeout_ms >= 1000 &&
           config->idle_timeout_ms < config->dim_timeout_ms &&
           config->dim_timeout_ms < config->sleep_timeout_ms &&
           config->sleep_timeout_ms < config->deep_sleep_timeout_ms &&
           config->active_brightness <= 100 &&
           config->dim_brightness <= 100;
}

static void session_config_load(void)
{
    session_config_t loaded = session_config;
    uint32_t value = 0;

    if (storage_manager_get_u32(SESSION_KEY_IDLE_TIMEOUT_MS, &value) == ESP_OK) {
        loaded.idle_timeout_ms = value;
    }
    if (storage_manager_get_u32(SESSION_KEY_DIM_TIMEOUT_MS, &value) == ESP_OK) {
        loaded.dim_timeout_ms = value;
    }
    if (storage_manager_get_u32(SESSION_KEY_SLEEP_TIMEOUT_MS, &value) == ESP_OK) {
        loaded.sleep_timeout_ms = value;
    }
    if (storage_manager_get_u32(SESSION_KEY_DEEP_SLEEP_TIMEOUT_MS, &value) == ESP_OK) {
        loaded.deep_sleep_timeout_ms = value;
    }
    if (storage_manager_get_u32(SESSION_KEY_ACTIVE_BRIGHTNESS, &value) == ESP_OK) {
        loaded.active_brightness = (uint8_t)value;
    }
    if (storage_manager_get_u32(SESSION_KEY_DIM_BRIGHTNESS, &value) == ESP_OK) {
        loaded.dim_brightness = (uint8_t)value;
    }

    if (session_config_is_valid(&loaded)) {
        session_config = loaded;
    } else {
        ESP_LOGW(TAG, "Ignoring invalid persisted session config");
    }
}

static esp_err_t session_config_save(const session_config_t *config)
{
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_IDLE_TIMEOUT_MS, config->idle_timeout_ms),
                        TAG, "save idle timeout failed");
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_DIM_TIMEOUT_MS, config->dim_timeout_ms),
                        TAG, "save dim timeout failed");
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_SLEEP_TIMEOUT_MS, config->sleep_timeout_ms),
                        TAG, "save sleep timeout failed");
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_DEEP_SLEEP_TIMEOUT_MS, config->deep_sleep_timeout_ms),
                        TAG, "save deep sleep timeout failed");
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_ACTIVE_BRIGHTNESS, config->active_brightness),
                        TAG, "save active brightness failed");
    ESP_RETURN_ON_ERROR(storage_manager_set_u32(SESSION_KEY_DIM_BRIGHTNESS, config->dim_brightness),
                        TAG, "save dim brightness failed");
    return ESP_OK;
}

static uint32_t session_idle_ms_now(void)
{
    int64_t elapsed_us = esp_timer_get_time() - last_activity_us;
    if (elapsed_us <= 0) {
        return 0;
    }
    return (uint32_t)(elapsed_us / 1000);
}

static void session_power_status_refresh(void)
{
    platform_power_status_t power = {0};
    if (platform_power_get_status(&power) == ESP_OK) {
        external_power_present = power.vbus_present;
        power_status_known = true;
    }
}

static void session_publish_locked(void)
{
    event_session_t event = {
        .state = (uint8_t)session.state,
        .reason = (uint8_t)session.reason,
        .brightness = session.brightness,
        .idle_ms = session.idle_ms,
    };
    event_bus_publish(EVENT_SESSION_STATE, &event, sizeof(event), 0);
}

static void session_apply_display_locked(void)
{
    if (!platform_display_is_ready()) {
        return;
    }

    if (session.state == SESSION_STATE_SLEEP_REQUESTED) {
        platform_display_set_backlight(false);
        return;
    }

    platform_display_set_backlight(true);
    platform_display_set_brightness(session.brightness);
}

static session_reason_t session_reason_from_wakeup(platform_wakeup_cause_t wakeup)
{
    switch (wakeup.reason) {
    case PLATFORM_WAKE_REASON_BOOT_BUTTON:
        return SESSION_REASON_INPUT;
    case PLATFORM_WAKE_REASON_POWER_BUTTON:
        return SESSION_REASON_POWER_BUTTON;
    case PLATFORM_WAKE_REASON_TOUCH:
        return SESSION_REASON_INPUT;
    case PLATFORM_WAKE_REASON_TIMER:
        return SESSION_REASON_TIMEOUT;
    case PLATFORM_WAKE_REASON_COLD_BOOT:
    case PLATFORM_WAKE_REASON_UNKNOWN:
    default:
        return SESSION_REASON_BOOT;
    }
}

static void session_transition_locked(session_state_t next, session_reason_t reason)
{
    if (session.state == next && session.reason == reason) {
        session.idle_ms = session_idle_ms_now();
        return;
    }

    session.state = next;
    session.reason = reason;
    session.idle_ms = session_idle_ms_now();

    switch (next) {
    case SESSION_STATE_ACTIVE:
    case SESSION_STATE_IDLE:
        session.brightness = session_config.active_brightness;
        break;
    case SESSION_STATE_DIM:
        session.brightness = session_config.dim_brightness;
        break;
    case SESSION_STATE_SLEEP_REQUESTED:
        session.brightness = 0;
        break;
    }

    session_apply_display_locked();
    session_publish_locked();
    ESP_LOGI(TAG, "State -> %s reason=%s idle=%lu ms brightness=%u",
             session_state_name(next), session_reason_name(reason),
             (unsigned long)session.idle_ms, session.brightness);

    if (next == SESSION_STATE_DIM) {
        ESP_LOGI(TAG, "Display dimmed to %u%% after %lu ms inactivity",
                 session.brightness, (unsigned long)session.idle_ms);
    } else if (next == SESSION_STATE_SLEEP_REQUESTED) {
        ESP_LOGI(TAG, "Display off, entering sleep stage after %lu ms inactivity",
                 (unsigned long)session.idle_ms);
    }
}

static void session_wake_locked(session_reason_t reason)
{
    last_activity_us = esp_timer_get_time();
    deep_sleep_retry_after_us = 0;
    ESP_LOGI(TAG, "Wake activity: %s", session_reason_name(reason));
    session_transition_locked(SESSION_STATE_ACTIVE, reason);
}

void session_manager_note_activity(session_reason_t reason)
{
    if (state_lock == NULL) {
        return;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    session_wake_locked(reason);
    xSemaphoreGive(state_lock);
}

static void session_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Deep sleep threshold reached at %lu ms inactivity",
             (unsigned long)session.idle_ms);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "Entering deep sleep now");
    esp_err_t err = platform_power_enter_deep_sleep(0);
    deep_sleep_retry_after_us = esp_timer_get_time() + (SESSION_DEEP_SLEEP_RETRY_MS * 1000LL);
    ESP_LOGE(TAG, "Deep sleep failed: %s", esp_err_to_name(err));
}

static void session_task(void *arg)
{
    (void)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(SESSION_POLL_MS));

        if (xSemaphoreTake(state_lock, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        session.idle_ms = session_idle_ms_now();

        if (session.state != SESSION_STATE_ACTIVE &&
            platform_touch_wakeup_pending()) {
            session_wake_locked(SESSION_REASON_INPUT);
            xSemaphoreGive(state_lock);
            continue;
        }

        const bool allow_deep_sleep = power_status_known ? !external_power_present : true;
        const bool should_deep_sleep =
            allow_deep_sleep &&
            session.idle_ms >= session_config.deep_sleep_timeout_ms &&
            (deep_sleep_retry_after_us == 0 || esp_timer_get_time() >= deep_sleep_retry_after_us);

        if (!allow_deep_sleep &&
            session.idle_ms >= session_config.deep_sleep_timeout_ms &&
            session.state == SESSION_STATE_SLEEP_REQUESTED) {
            static int64_t last_skip_log_us;
            const int64_t now_us = esp_timer_get_time();
            if (now_us - last_skip_log_us >= 10000000LL) {
                ESP_LOGI(TAG, "Deep sleep skipped: external power present");
                last_skip_log_us = now_us;
            }
        }

        if (should_deep_sleep) {
            session_transition_locked(SESSION_STATE_SLEEP_REQUESTED, SESSION_REASON_TIMEOUT);
            xSemaphoreGive(state_lock);
            session_enter_deep_sleep();
            continue;
        } else if (session.idle_ms >= session_config.sleep_timeout_ms &&
                   session.state != SESSION_STATE_SLEEP_REQUESTED) {
            session_transition_locked(SESSION_STATE_SLEEP_REQUESTED, SESSION_REASON_TIMEOUT);
        } else if (session.idle_ms >= session_config.dim_timeout_ms &&
                   session.state != SESSION_STATE_DIM &&
                   session.state != SESSION_STATE_SLEEP_REQUESTED) {
            session_transition_locked(SESSION_STATE_DIM, SESSION_REASON_TIMEOUT);
        } else if (session.idle_ms >= session_config.idle_timeout_ms &&
                   session.state == SESSION_STATE_ACTIVE) {
            session_transition_locked(SESSION_STATE_IDLE, SESSION_REASON_TIMEOUT);
        }

        xSemaphoreGive(state_lock);
    }
}

static void power_task(void *arg)
{
    (void)arg;

    while (true) {
        platform_power_event_t event;
        esp_err_t err = platform_power_get_event(&event, 1000);
        if (err != ESP_OK) {
            continue;
        }

        external_power_present = event.status.vbus_present;
        power_status_known = true;

        session_manager_note_activity(SESSION_REASON_POWER_EVENT);
    }
}

esp_err_t session_manager_init(void)
{
    if (state_lock != NULL) {
        return ESP_OK;
    }

    state_lock = xSemaphoreCreateMutex();
    if (state_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    last_activity_us = esp_timer_get_time();
    deep_sleep_retry_after_us = 0;
    external_power_present = false;
    power_status_known = false;
    session_config_load();
    session_power_status_refresh();
    const platform_wakeup_cause_t wakeup = platform_power_get_wakeup_cause();

    if (xSemaphoreTake(state_lock, portMAX_DELAY) == pdTRUE) {
        memset(&session, 0, sizeof(session));
        session.state = SESSION_STATE_ACTIVE;
        session.reason = session_reason_from_wakeup(wakeup);
        session.brightness = session_config.active_brightness;
        session_apply_display_locked();
        session_publish_locked();
        xSemaphoreGive(state_lock);
    }

    if (xTaskCreate(session_task, "session_task", 3072, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(power_task, "power_task", 3072, NULL, 4, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool session_manager_handle_input(const platform_input_event_t *input)
{
    if (input == NULL || state_lock == NULL) {
        return false;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    const bool consume_for_wake = (session.state == SESSION_STATE_SLEEP_REQUESTED);
    xSemaphoreGive(state_lock);
    session_manager_note_activity(SESSION_REASON_INPUT);

    if (input->input == PLATFORM_INPUT_POWER &&
        input->action == PLATFORM_INPUT_LONG_PRESS) {
        ESP_LOGW(TAG, "Power long press detected, shutting down");
        session_manager_request_power_off();
        return true;
    }

    return consume_for_wake;
}

esp_err_t session_manager_request_power_off(void)
{
    if (state_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) == pdTRUE) {
        session_transition_locked(SESSION_STATE_SLEEP_REQUESTED, SESSION_REASON_POWER_BUTTON);
        xSemaphoreGive(state_lock);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    return platform_power_off();
}

session_status_t session_manager_get_status(void)
{
    session_status_t snapshot = session;
    if (state_lock == NULL) {
        return snapshot;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) == pdTRUE) {
        snapshot = session;
        snapshot.idle_ms = session_idle_ms_now();
        xSemaphoreGive(state_lock);
    }
    return snapshot;
}

session_config_t session_manager_get_config(void)
{
    session_config_t snapshot = session_config;
    if (state_lock == NULL) {
        return snapshot;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) == pdTRUE) {
        snapshot = session_config;
        xSemaphoreGive(state_lock);
    }
    return snapshot;
}

esp_err_t session_manager_set_config(const session_config_t *config)
{
    if (!session_config_is_valid(config)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (state_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(state_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    session_config = *config;
    if (session.state == SESSION_STATE_ACTIVE || session.state == SESSION_STATE_IDLE) {
        session.brightness = session_config.active_brightness;
    } else if (session.state == SESSION_STATE_DIM) {
        session.brightness = session_config.dim_brightness;
    }
    session_apply_display_locked();
    session_publish_locked();
    xSemaphoreGive(state_lock);

    return session_config_save(config);
}
