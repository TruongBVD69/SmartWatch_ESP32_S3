#include "smartwatch_platform.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

ESP_EVENT_DEFINE_BASE(SMARTWATCH_EVENTS);

static const char *TAG = "platform";
static bool initialized;

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt-watchdog";
    case ESP_RST_TASK_WDT: return "task-watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep-sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
    }
}

esp_err_t smartwatch_platform_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    ESP_LOGI(TAG, "Reset reason: %s", reset_reason_name(esp_reset_reason()));
    initialized = true;
    return smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_PLATFORM,
                                                     SMARTWATCH_SERVICE_READY, ESP_OK);
}

esp_err_t smartwatch_platform_post(int32_t event_id, const void *data, size_t data_size,
                                   uint32_t timeout_ms)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_event_post(SMARTWATCH_EVENTS, event_id, data, data_size, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t smartwatch_platform_publish_service_state(smartwatch_service_id_t service,
                                                    smartwatch_service_state_t state,
                                                    esp_err_t error)
{
    const smartwatch_service_event_t event = {
        .service = service,
        .state = state,
        .error = error,
    };
    if (state == SMARTWATCH_SERVICE_DEGRADED || state == SMARTWATCH_SERVICE_FAILED) {
        ESP_LOGW(TAG, "%s is %s: %s", smartwatch_platform_service_name(service),
                 smartwatch_platform_service_state_name(state), esp_err_to_name(error));
    } else {
        ESP_LOGI(TAG, "%s is %s", smartwatch_platform_service_name(service),
                 smartwatch_platform_service_state_name(state));
    }
    return smartwatch_platform_post(SMARTWATCH_EVENT_SERVICE_STATE, &event, sizeof(event), 100);
}

const char *smartwatch_platform_service_name(smartwatch_service_id_t service)
{
    static const char *const names[] = {
        "platform", "storage", "board", "power", "input", "rtc", "health", "ui",
    };
    return service >= 0 && service < (int)(sizeof(names) / sizeof(names[0])) ? names[service] : "unknown";
}

const char *smartwatch_platform_service_state_name(smartwatch_service_state_t state)
{
    static const char *const names[] = {"starting", "ready", "degraded", "failed"};
    return state >= 0 && state < (int)(sizeof(names) / sizeof(names[0])) ? names[state] : "unknown";
}

esp_err_t smartwatch_platform_watchdog_subscribe(void)
{
    esp_err_t err = esp_task_wdt_add(NULL);
    return err == ESP_ERR_INVALID_STATE ? ESP_OK : err;
}

esp_err_t smartwatch_platform_watchdog_feed(void)
{
    esp_err_t err = esp_task_wdt_reset();
    return err == ESP_ERR_NOT_FOUND ? ESP_OK : err;
}

void smartwatch_platform_watchdog_unsubscribe(void)
{
    esp_task_wdt_delete(NULL);
}
