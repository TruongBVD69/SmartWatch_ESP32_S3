#include "diagnostics_app.h"

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "audio_service.h"
#include "battery_service.h"
#include "esp_check.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform.h"
#include "platform_input.h"
#include "screen_manager.h"
#include "sensor_service.h"
#include "service_manager.h"
#include "session_manager.h"
#include "storage_manager.h"
#include "storage_service.h"
#include "time_service.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_theme.h"

static lv_obj_t *screen;
static lv_obj_t *status_label;
static lv_obj_t *audio_button;
static lv_obj_t *refresh_button;
static TaskHandle_t diagnostics_task_handle;
static uint32_t diagnostics_run_id;

typedef struct {
    bool touch_seen;
    bool boot_seen;
    bool power_seen;
    bool speaker_pass;
    bool mic_pass;
    int16_t mic_min;
    int16_t mic_max;
    event_input_t last_input;
    event_session_t last_session;
} diagnostics_runtime_t;

static diagnostics_runtime_t runtime_state;

#define MIC_SIGNAL_RANGE_MIN 20
#define MIC_WARMUP_MS 250
#define MIC_READ_PASSES 4

static void diagnostics_task(void *arg);
static esp_err_t diagnostics_enter(void *param);
static esp_err_t diagnostics_pause(void);
static esp_err_t diagnostics_exit(void);
static esp_err_t diagnostics_resume(void);
static void diagnostics_refresh(lv_event_t *event);
static void diagnostics_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data);
static void diagnostics_touch_event(lv_event_t *event);
static void diagnostics_schedule_refresh(void);

static const char *service_state_name(service_state_t state)
{
    switch (state) {
    case SERVICE_STATE_STOPPED: return "STOPPED";
    case SERVICE_STATE_INITIALIZING: return "INIT";
    case SERVICE_STATE_STARTING: return "START";
    case SERVICE_STATE_READY: return "READY";
    case SERVICE_STATE_DEGRADED: return "DEGRADED";
    case SERVICE_STATE_FAILED: return "FAILED";
    }
    return "?";
}

static const char *session_state_name(session_state_t state)
{
    switch (state) {
    case SESSION_STATE_ACTIVE: return "ACTIVE";
    case SESSION_STATE_IDLE: return "IDLE";
    case SESSION_STATE_DIM: return "DIM";
    case SESSION_STATE_SLEEP_REQUESTED: return "SLEEP_REQ";
    }
    return "?";
}

static const char *session_reason_name(session_reason_t reason)
{
    switch (reason) {
    case SESSION_REASON_BOOT: return "BOOT";
    case SESSION_REASON_INPUT: return "INPUT";
    case SESSION_REASON_TIMEOUT: return "TIMEOUT";
    case SESSION_REASON_POWER_EVENT: return "POWER_EVT";
    case SESSION_REASON_POWER_BUTTON: return "POWER_BTN";
    }
    return "?";
}

static void diagnostics_set_text(const char *text)
{
    if (!ui_manager_lock(500)) return;
    lv_label_set_text(status_label, text);
    ui_manager_unlock();
}

static void diagnostics_reset_run(void)
{
    diagnostics_run_id++;
    diagnostics_task_handle = NULL;
}

static void diagnostics_schedule_refresh(void)
{
    if (app_manager_current() == NULL ||
        app_manager_current()->id != APP_ID_DIAGNOSTICS ||
        diagnostics_task_handle != NULL) {
        return;
    }
    diagnostics_refresh(NULL);
}

static size_t append_line(char *buffer, size_t used, size_t capacity, const char *fmt, ...)
{
    if (used >= capacity) return used;
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buffer + used, capacity - used, fmt, args);
    va_end(args);
    if (written < 0) return used;
    size_t next = used + (size_t)written;
    return next < capacity ? next : capacity - 1;
}

static esp_err_t run_speaker_test(void)
{
    const platform_audio_config_t config = PLATFORM_AUDIO_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(audio_service_start(&config, true, false), "diagnostics", "speaker start failed");
    audio_service_set_volume(75.0f);

    int16_t samples[1600];
    const size_t period = config.sample_rate_hz / 880U;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        samples[i] = ((i % period) < (period / 2U)) ? 12000 : -12000;
    }

    esp_err_t err = ESP_OK;
    for (size_t burst = 0; burst < 4; ++burst) {
        err = audio_service_write(samples, sizeof(samples));
        if (err != ESP_OK) {
            break;
        }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_err_t stop_err = audio_service_stop();
    return err != ESP_OK ? err : stop_err;
}

static esp_err_t run_microphone_test(int16_t *min_sample, int16_t *max_sample)
{
    if (min_sample == NULL || max_sample == NULL) return ESP_ERR_INVALID_ARG;
    const platform_audio_config_t config = PLATFORM_AUDIO_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(audio_service_start(&config, false, true), "diagnostics", "mic start failed");
    audio_service_set_input_gain(24.0f);

    int16_t samples[800];
    vTaskDelay(pdMS_TO_TICKS(MIC_WARMUP_MS));

    esp_err_t err = ESP_OK;
    bool initialized = false;
    for (size_t pass = 0; pass < MIC_READ_PASSES; ++pass) {
        err = audio_service_read(samples, sizeof(samples));
        if (err != ESP_OK) {
            break;
        }
        for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
            if (!initialized) {
                *min_sample = samples[i];
                *max_sample = samples[i];
                initialized = true;
            } else {
                if (samples[i] < *min_sample) *min_sample = samples[i];
                if (samples[i] > *max_sample) *max_sample = samples[i];
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    esp_err_t stop_err = audio_service_stop();
    if (err != ESP_OK) return err;
    if (!initialized) return ESP_ERR_INVALID_RESPONSE;
    return stop_err;
}

static void audio_test_task(void *arg)
{
    const uint32_t run_id = (uint32_t)(uintptr_t)arg;
    char text[512];
    size_t used = 0;

    esp_err_t speaker_err = run_speaker_test();
    used = append_line(text, used, sizeof(text), "Speaker self-test: %s\n",
                       speaker_err == ESP_OK ? "PASS" : esp_err_to_name(speaker_err));

    int16_t mic_min = 0;
    int16_t mic_max = 0;
    esp_err_t mic_err = run_microphone_test(&mic_min, &mic_max);
    const int mic_range = mic_max - mic_min;
    const bool mic_signal_ok = mic_err == ESP_OK && mic_range >= MIC_SIGNAL_RANGE_MIN;
    runtime_state.speaker_pass = (speaker_err == ESP_OK);
    runtime_state.mic_pass = mic_signal_ok;
    runtime_state.mic_min = mic_min;
    runtime_state.mic_max = mic_max;
    used = append_line(text, used, sizeof(text), "Mic self-test: %s (%d..%d)\n",
                       mic_signal_ok ? "PASS"
                                     : (mic_err == ESP_OK ? "FAIL: no signal"
                                                          : esp_err_to_name(mic_err)),
                       mic_min, mic_max);

    if (run_id == diagnostics_run_id && ui_manager_lock(500)) {
        lv_label_set_text(status_label, text);
        lv_obj_clear_state(audio_button, LV_STATE_DISABLED);
        lv_obj_clear_state(refresh_button, LV_STATE_DISABLED);
        ui_manager_unlock();
    }
    vTaskDelete(NULL);
}

static void diagnostics_back(lv_event_t *event)
{
    (void)event;
    ui_events_back();
}

static void diagnostics_touch_event(lv_event_t *event)
{
    (void)event;
    runtime_state.touch_seen = true;
    diagnostics_schedule_refresh();
}

static void diagnostics_run_audio(lv_event_t *event)
{
    (void)event;
    if (!ui_manager_lock(200)) return;
    lv_obj_add_state(audio_button, LV_STATE_DISABLED);
    lv_obj_add_state(refresh_button, LV_STATE_DISABLED);
    lv_label_set_text(status_label, "Running audio diagnostics...\n");
    ui_manager_unlock();
    xTaskCreate(audio_test_task, "diag_audio", 6144, (void *)(uintptr_t)diagnostics_run_id, 4, NULL);
}

static size_t append_history(char *buffer, size_t used, size_t capacity)
{
    used = append_line(buffer, used, capacity, "History:");
    const size_t depth = app_manager_history_depth();
    if (depth == 0) {
        return append_line(buffer, used, capacity, " empty\n");
    }

    used = append_line(buffer, used, capacity, "\n");
    for (size_t i = 0; i < depth; ++i) {
        const app_t *app = app_manager_history_peek(i);
        used = append_line(buffer, used, capacity, "  %u. %s\n",
                           (unsigned)(i + 1), app != NULL ? app->name : "?");
    }
    return used;
}

static void diagnostics_refresh(lv_event_t *event)
{
    (void)event;
    if (diagnostics_task_handle != NULL) {
        diagnostics_set_text("Diagnostics refresh already running...\n");
        return;
    }

    diagnostics_set_text("Running diagnostics...\n");
    if (xTaskCreate(diagnostics_task, "diagnostics", 6144,
                    (void *)(uintptr_t)diagnostics_run_id, 4,
                    &diagnostics_task_handle) != pdPASS) {
        diagnostics_task_handle = NULL;
        diagnostics_set_text("Failed to start diagnostics task.\n");
    }
}

static void diagnostics_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;

    if (id == EVENT_UI_ACTION && data != NULL) {
        const event_input_t *input = data;
        runtime_state.last_input = *input;
        if (input->input == PLATFORM_INPUT_BOOT) {
            runtime_state.boot_seen = true;
        } else if (input->input == PLATFORM_INPUT_POWER) {
            runtime_state.power_seen = true;
        }
        diagnostics_schedule_refresh();
        return;
    }

    if (id == EVENT_SESSION_STATE && data != NULL) {
        runtime_state.last_session = *(const event_session_t *)data;
        diagnostics_schedule_refresh();
    }
}

static void diagnostics_task(void *arg)
{
    const uint32_t run_id = (uint32_t)(uintptr_t)arg;
    char text[2200];
    size_t used = 0;

    platform_datetime_t time;
    platform_power_status_t battery;
    platform_imu_sample_t imu;
    const session_status_t session = session_manager_get_status();
    const session_config_t session_cfg = session_manager_get_config();
    const platform_status_t *platform = platform_get_status();
    const storage_manager_status_t *storage = storage_manager_status();
    const bool rtc_ok = time_service_get(&time) == ESP_OK;
    const bool battery_ok = battery_service_get(&battery) == ESP_OK;
    const bool imu_ok = sensor_service_read(&imu) == ESP_OK;
    const char *last_input_name = "-";
    if (runtime_state.last_input.input == PLATFORM_INPUT_BOOT) {
        last_input_name = "BOOT";
    } else if (runtime_state.last_input.input == PLATFORM_INPUT_POWER) {
        last_input_name = "POWER";
    }
    const char *last_action_name = "-";
    switch (runtime_state.last_input.action) {
    case PLATFORM_INPUT_PRESS: last_action_name = "PRESS"; break;
    case PLATFORM_INPUT_RELEASE: last_action_name = "RELEASE"; break;
    case PLATFORM_INPUT_CLICK: last_action_name = "CLICK"; break;
    case PLATFORM_INPUT_LONG_PRESS: last_action_name = "LONG"; break;
    }

    used = append_line(text, used, sizeof(text),
                       "App: %s\nBack stack: %s (%u)\nAssets: %s\nSD mounted: %s\n\n",
                       app_manager_current() != NULL ? app_manager_current()->name : "-",
                       app_manager_can_go_back() ? "available" : "empty",
                       (unsigned)app_manager_history_depth(),
                       storage->assets_ready ? "ready" : "missing",
                       storage_service_sd_is_mounted() ? "YES" : "NO");
    used = append_history(text, used, sizeof(text));
    used = append_line(text, used, sizeof(text), "\n");
    used = append_line(text, used, sizeof(text),
                       "Display: %s  Touch: %s\nRTC: %s  IMU: %s\nPower: %s  Buttons: %s\n\n",
                       platform->display_ready ? "OK" : "FAIL",
                       platform->touch_ready ? "OK" : "FAIL",
                       platform->rtc_ready ? "OK" : "FAIL",
                       platform->imu_ready ? "OK" : "FAIL",
                       platform->power_ready ? "OK" : "FAIL",
                       platform->buttons_ready ? "OK" : "FAIL");
    used = append_line(text, used, sizeof(text),
                       "RTC read: %s\nBattery read: %s\nIMU read: %s\n",
                       rtc_ok ? "OK" : "FAIL",
                       battery_ok ? "OK" : "FAIL",
                       imu_ok ? "OK" : "FAIL");
    used = append_line(text, used, sizeof(text),
                       "Touch live: %s\nBOOT button live: %s\nPWR button live: %s\n"
                       "Last input: %s %s (%lums)\n",
                       runtime_state.touch_seen ? "PASS" : "WAIT",
                       runtime_state.boot_seen ? "PASS" : "WAIT",
                       runtime_state.power_seen ? "PASS" : "WAIT",
                       last_input_name, last_action_name,
                       (unsigned long)runtime_state.last_input.duration_ms);
    used = append_line(text, used, sizeof(text),
                       "Session: %s via %s idle=%lums br=%u%%\n"
                       "Last event: %s via %s idle=%lums br=%u%%\n"
                       "Timeouts: idle=%lus dim=%lus sleep=%lus\n",
                       session_state_name(session.state),
                       session_reason_name(session.reason),
                       (unsigned long)session.idle_ms,
                       session.brightness,
                       session_state_name((session_state_t)runtime_state.last_session.state),
                       session_reason_name((session_reason_t)runtime_state.last_session.reason),
                       (unsigned long)runtime_state.last_session.idle_ms,
                       runtime_state.last_session.brightness,
                       (unsigned long)(session_cfg.idle_timeout_ms / 1000),
                       (unsigned long)(session_cfg.dim_timeout_ms / 1000),
                       (unsigned long)(session_cfg.sleep_timeout_ms / 1000));

    diagnostics_set_text(text);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t sd_err = storage_service_mount_sd();
    used = append_line(text, used, sizeof(text), "SD self-test: %s\n",
                       sd_err == ESP_OK ? "PASS" : esp_err_to_name(sd_err));
    diagnostics_set_text(text);
    vTaskDelay(pdMS_TO_TICKS(100));

    used = append_line(text, used, sizeof(text),
                       "Audio self-test: manual\n"
                       "Speaker: %s\nMic: %s (%d..%d)\n"
                       "Use the Audio button to rerun.\n\n",
                       runtime_state.speaker_pass ? "PASS" : "WAIT",
                       runtime_state.mic_pass ? "PASS" : "WAIT",
                       runtime_state.mic_min, runtime_state.mic_max);

    used = append_line(text, used, sizeof(text), "Services:\n");
    const char *names[] = {"battery", "time", "sensor", "audio", "storage"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const service_runtime_info_t *info = service_manager_find(names[i]);
        if (info == NULL) continue;
        used = append_line(text, used, sizeof(text),
                           "%s=%s err=%s fail=%u restart=%u\n",
                           info->name, service_state_name(info->state),
                           esp_err_to_name(info->last_error),
                           info->failure_count, info->restart_count);
    }

    if (run_id == diagnostics_run_id) {
        if (ui_manager_lock(500)) {
            lv_label_set_text(status_label, text);
            lv_obj_clear_state(audio_button, LV_STATE_DISABLED);
            lv_obj_clear_state(refresh_button, LV_STATE_DISABLED);
            ui_manager_unlock();
        }
    }
    diagnostics_task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t diagnostics_init(void)
{
    if (!ui_manager_lock(1000)) return ESP_ERR_TIMEOUT;
    screen = ui_manager_create_screen();
    lv_obj_add_event_cb(screen, diagnostics_touch_event, LV_EVENT_PRESSED, NULL);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Diagnostics");
    lv_obj_set_style_text_color(title, ui_theme_text_primary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    status_label = lv_label_create(screen);
    lv_obj_set_width(status_label, 330);
    lv_obj_set_style_text_color(status_label, ui_theme_text_secondary(), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 40, 90);

    audio_button = lv_button_create(screen);
    lv_obj_set_size(audio_button, 104, 42);
    lv_obj_align(audio_button, LV_ALIGN_BOTTOM_LEFT, 24, -28);
    lv_obj_add_event_cb(audio_button, diagnostics_run_audio, LV_EVENT_CLICKED, NULL);
    lv_obj_t *audio_label = lv_label_create(audio_button);
    lv_label_set_text(audio_label, "Audio");
    lv_obj_center(audio_label);

    refresh_button = lv_button_create(screen);
    lv_obj_set_size(refresh_button, 104, 42);
    lv_obj_align(refresh_button, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_add_event_cb(refresh_button, diagnostics_refresh, LV_EVENT_CLICKED, NULL);
    lv_obj_t *refresh_label = lv_label_create(refresh_button);
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_center(refresh_label);

    lv_obj_t *back_button = lv_button_create(screen);
    lv_obj_set_size(back_button, 104, 42);
    lv_obj_align(back_button, LV_ALIGN_BOTTOM_RIGHT, -24, -28);
    lv_obj_add_event_cb(back_button, diagnostics_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_button);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    ui_manager_unlock();
    event_bus_subscribe(EVENT_UI_ACTION, diagnostics_event_handler, NULL);
    event_bus_subscribe(EVENT_SESSION_STATE, diagnostics_event_handler, NULL);
    return ESP_OK;
}

static esp_err_t diagnostics_enter(void *param)
{
    (void)param;
    diagnostics_reset_run();
    memset(&runtime_state, 0, sizeof(runtime_state));
    runtime_state.last_session = (event_session_t) {
        .state = SESSION_STATE_ACTIVE,
        .reason = SESSION_REASON_BOOT,
        .brightness = 100,
        .idle_ms = 0,
    };
    ESP_RETURN_ON_ERROR(screen_manager_show(screen), "diagnostics", "show failed");
    diagnostics_refresh(NULL);
    return ESP_OK;
}

static esp_err_t diagnostics_pause(void)
{
    diagnostics_reset_run();
    return ESP_OK;
}

static esp_err_t diagnostics_exit(void)
{
    diagnostics_reset_run();
    return ESP_OK;
}

static esp_err_t diagnostics_resume(void)
{
    return diagnostics_enter(NULL);
}

const app_t *diagnostics_app_descriptor(void)
{
    static const app_t app = {
        .id = APP_ID_DIAGNOSTICS, .name = "Diagnostics",
        .init = diagnostics_init,
        .enter = diagnostics_enter,
        .pause = diagnostics_pause,
        .exit = diagnostics_exit,
        .resume = diagnostics_resume,
    };
    return &app;
}
