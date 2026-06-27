#include "smartwatch_services.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "smartwatch_board.h"
#include "smartwatch_platform.h"

static const char *TAG = "services";

typedef struct {
    smartwatch_service_id_t id;
    smartwatch_service_state_t state;
    unsigned failures;
} service_runtime_t;

static service_runtime_t power_runtime = {.id = SMARTWATCH_SERVICE_POWER, .state = SMARTWATCH_SERVICE_STARTING};
static service_runtime_t input_runtime = {.id = SMARTWATCH_SERVICE_INPUT, .state = SMARTWATCH_SERVICE_STARTING};
static service_runtime_t rtc_runtime = {.id = SMARTWATCH_SERVICE_RTC, .state = SMARTWATCH_SERVICE_STARTING};
static service_runtime_t health_runtime = {.id = SMARTWATCH_SERVICE_HEALTH, .state = SMARTWATCH_SERVICE_STARTING};

static void service_result(service_runtime_t *runtime, esp_err_t result)
{
    smartwatch_service_state_t next = SMARTWATCH_SERVICE_READY;
    if (result != ESP_OK) {
        runtime->failures++;
        next = runtime->failures >= 3 ? SMARTWATCH_SERVICE_DEGRADED : runtime->state;
    } else {
        runtime->failures = 0;
    }
    if (next != runtime->state) {
        runtime->state = next;
        smartwatch_platform_publish_service_state(runtime->id, next, result);
    }
}

static smartwatch_power_event_t make_power_event(const smartwatch_board_power_status_t *status)
{
    return (smartwatch_power_event_t) {
        .battery_present = status->battery_present,
        .vbus_present = status->vbus_present,
        .charging = status->charging,
        .battery_percent = status->battery_percent,
        .battery_mv = status->battery_mv,
        .pmic_temperature_c = status->pmic_temperature_c,
    };
}

static void power_task(void *arg)
{
    (void)arg;
    smartwatch_platform_watchdog_subscribe();
    smartwatch_board_power_status_t status = {0};
    esp_err_t err = smartwatch_board_power_get_status(&status);
    service_result(&power_runtime, err);
    if (err == ESP_OK) {
        smartwatch_power_event_t event = make_power_event(&status);
        smartwatch_platform_post(SMARTWATCH_EVENT_POWER_STATUS, &event, sizeof(event), 100);
    }

    TickType_t last_snapshot = xTaskGetTickCount();
    while (true) {
        smartwatch_board_power_event_t board_event;
        err = smartwatch_board_power_get_event(&board_event, 1000);
        if (err == ESP_OK) {
            smartwatch_power_event_t event = make_power_event(&board_event.status);
            smartwatch_platform_post(SMARTWATCH_EVENT_POWER_CHANGED, &event, sizeof(event), 100);
            service_result(&power_runtime, ESP_OK);
        } else if (err != ESP_ERR_TIMEOUT) {
            service_result(&power_runtime, err);
            vTaskDelay(pdMS_TO_TICKS(250));
        }

        const uint32_t snapshot_period_ms = power_runtime.state == SMARTWATCH_SERVICE_READY ? 30000 : 5000;
        if (xTaskGetTickCount() - last_snapshot >= pdMS_TO_TICKS(snapshot_period_ms)) {
            err = smartwatch_board_power_get_status(&status);
            service_result(&power_runtime, err);
            if (err == ESP_OK) {
                smartwatch_power_event_t event = make_power_event(&status);
                smartwatch_platform_post(SMARTWATCH_EVENT_POWER_STATUS, &event, sizeof(event), 100);
            }
            last_snapshot = xTaskGetTickCount();
        }
        smartwatch_platform_watchdog_feed();
    }
}

static void input_task(void *arg)
{
    (void)arg;
    smartwatch_platform_watchdog_subscribe();
    service_result(&input_runtime, ESP_OK);
    while (true) {
        smartwatch_board_button_event_t board_event;
        esp_err_t err = smartwatch_board_button_get_event(&board_event, 1000);
        if (err == ESP_OK) {
            const smartwatch_input_event_t event = {
                .input = board_event.button == SMARTWATCH_BUTTON_POWER
                             ? SMARTWATCH_INPUT_POWER : SMARTWATCH_INPUT_BOOT,
                .action = (smartwatch_input_action_t)board_event.type,
                .duration_ms = board_event.duration_ms,
            };
            smartwatch_platform_post(SMARTWATCH_EVENT_BUTTON, &event, sizeof(event), 100);
            service_result(&input_runtime, ESP_OK);
        } else if (err != ESP_ERR_TIMEOUT) {
            service_result(&input_runtime, err);
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        smartwatch_platform_watchdog_feed();
    }
}

static void rtc_task(void *arg)
{
    (void)arg;
    smartwatch_platform_watchdog_subscribe();
    pcf85063a_dev_t *rtc = smartwatch_board_get_rtc();
    while (true) {
        pcf85063a_datetime_t now = {0};
        esp_err_t err = rtc == NULL ? ESP_ERR_INVALID_STATE : pcf85063a_get_time_date(rtc, &now);
        service_result(&rtc_runtime, err);
        if (err == ESP_OK) {
            const smartwatch_rtc_event_t event = {
                .year = now.year,
                .month = now.month,
                .day = now.day,
                .weekday = now.dotw,
                .hour = now.hour,
                .minute = now.min,
                .second = now.sec,
            };
            smartwatch_platform_post(SMARTWATCH_EVENT_RTC_TICK, &event, sizeof(event), 100);
        }
        smartwatch_platform_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void health_task(void *arg)
{
    (void)arg;
    smartwatch_platform_watchdog_subscribe();
    service_result(&health_runtime, ESP_OK);
    TickType_t last_report = 0;
    while (true) {
        const TickType_t now = xTaskGetTickCount();
        if (now - last_report >= pdMS_TO_TICKS(30000) || last_report == 0) {
            const smartwatch_health_event_t event = {
                .free_heap = esp_get_free_heap_size(),
                .minimum_free_heap = esp_get_minimum_free_heap_size(),
                .free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            };
            smartwatch_platform_post(SMARTWATCH_EVENT_SYSTEM_HEALTH, &event, sizeof(event), 100);
            ESP_LOGI(TAG, "Health: heap=%lu min=%lu psram=%lu",
                     (unsigned long)event.free_heap, (unsigned long)event.minimum_free_heap,
                     (unsigned long)event.free_psram);
            last_report = now;
        }
        smartwatch_platform_watchdog_feed();
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

static esp_err_t start_task(TaskFunction_t function, const char *name, uint32_t stack_size,
                            UBaseType_t priority, service_runtime_t *runtime)
{
    smartwatch_platform_publish_service_state(runtime->id, SMARTWATCH_SERVICE_STARTING, ESP_OK);
    if (xTaskCreate(function, name, stack_size, NULL, priority, NULL) != pdPASS) {
        runtime->state = SMARTWATCH_SERVICE_FAILED;
        smartwatch_platform_publish_service_state(runtime->id, SMARTWATCH_SERVICE_FAILED, ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t smartwatch_services_start(void)
{
    const smartwatch_board_status_t *board = smartwatch_board_get_status();
    esp_err_t result = ESP_OK;

    if (board->power_ready) {
        if (start_task(power_task, "svc_power", 4096, 5, &power_runtime) != ESP_OK) result = ESP_FAIL;
    } else {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_POWER,
                                                  SMARTWATCH_SERVICE_DEGRADED, ESP_ERR_NOT_FOUND);
    }
    if (board->buttons_ready) {
        if (start_task(input_task, "svc_input", 3072, 5, &input_runtime) != ESP_OK) result = ESP_FAIL;
    } else {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_INPUT,
                                                  SMARTWATCH_SERVICE_DEGRADED, ESP_ERR_NOT_FOUND);
    }
    if (board->rtc_ready) {
        if (start_task(rtc_task, "svc_rtc", 3072, 4, &rtc_runtime) != ESP_OK) result = ESP_FAIL;
    } else {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_RTC,
                                                  SMARTWATCH_SERVICE_DEGRADED, ESP_ERR_NOT_FOUND);
    }
    if (start_task(health_task, "svc_health", 3072, 3, &health_runtime) != ESP_OK) result = ESP_FAIL;
    return result;
}
