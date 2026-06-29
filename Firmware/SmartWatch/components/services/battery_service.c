#include "battery_service.h"

#include "esp_task_wdt.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

static const char *TAG = "battery_service";
static platform_power_status_t latest;
static bool ready;
static TaskHandle_t battery_task_handle;

esp_err_t battery_service_get(platform_power_status_t *status)
{
    if (status == NULL) return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    *status = latest;
    return ESP_OK;
}

static void battery_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);
    while (true) {
        esp_err_t err = platform_power_get_status(&latest);
        if (err == ESP_OK) {
            ready = true;
            const event_battery_t event = {
                .present = latest.battery_present,
                .charging = latest.charging,
                .percent = latest.battery_percent,
                .millivolts = latest.battery_mv,
            };
            event_bus_publish(EVENT_BATTERY_UPDATED, &event, sizeof(event), 100);
        } else {
            LOGW(TAG, "PMIC read failed: %s", esp_err_to_name(err));
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    battery_task_handle = NULL;
}

static esp_err_t battery_init(void)
{
    ready = platform_power_get_status(&latest) == ESP_OK;
    return ESP_OK;
}

static esp_err_t battery_start(void)
{
    if (battery_task_handle != NULL) {
        return ESP_OK;
    }
    return xTaskCreate(battery_task, "battery", 3072, NULL, 4, &battery_task_handle) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t battery_stop(void)
{
    if (battery_task_handle != NULL) {
        esp_task_wdt_delete(battery_task_handle);
        vTaskDelete(battery_task_handle);
        battery_task_handle = NULL;
    }
    ready = false;
    return ESP_OK;
}

static esp_err_t battery_health_check(void)
{
    platform_power_status_t status = {0};
    return platform_power_get_status(&status);
}

const service_t *battery_service_descriptor(void)
{
    static const service_t service = {
        .name = "battery",
        .init = battery_init,
        .start = battery_start,
        .stop = battery_stop,
        .health_check = battery_health_check,
        .critical = false,
        .auto_recover = true,
    };
    return &service;
}
