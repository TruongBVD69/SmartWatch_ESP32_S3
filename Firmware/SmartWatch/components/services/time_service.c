#include "time_service.h"

#include "esp_task_wdt.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

static const char *TAG = "time_service";
static platform_datetime_t latest;
static bool ready;
static TaskHandle_t time_task_handle;

esp_err_t time_service_get(platform_datetime_t *datetime)
{
    if (datetime == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = platform_rtc_get(datetime);
    if (err == ESP_OK) {
        latest = *datetime;
        ready = true;
    }
    return err;
}

esp_err_t time_service_set(const platform_datetime_t *datetime)
{
    return platform_rtc_set(datetime);
}

static void time_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);
    while (true) {
        esp_err_t err = platform_rtc_get(&latest);
        if (err == ESP_OK) {
            ready = true;
            const event_time_t event = {
                .year = latest.year, .month = latest.month, .day = latest.day,
                .hour = latest.hour, .minute = latest.minute, .second = latest.second,
            };
            event_bus_publish(EVENT_TIME_UPDATED, &event, sizeof(event), 100);
        } else {
            LOGW(TAG, "RTC read failed: %s", esp_err_to_name(err));
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    time_task_handle = NULL;
}

static esp_err_t time_init(void)
{
    ready = platform_rtc_get(&latest) == ESP_OK;
    return ESP_OK;
}

static esp_err_t time_start(void)
{
    if (time_task_handle != NULL) {
        return ESP_OK;
    }
    return xTaskCreate(time_task, "time", 3072, NULL, 4, &time_task_handle) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t time_stop(void)
{
    if (time_task_handle != NULL) {
        esp_task_wdt_delete(time_task_handle);
        vTaskDelete(time_task_handle);
        time_task_handle = NULL;
    }
    ready = false;
    return ESP_OK;
}

static esp_err_t time_health_check(void)
{
    platform_datetime_t datetime = {0};
    return platform_rtc_get(&datetime);
}

const service_t *time_service_descriptor(void)
{
    static const service_t service = {
        .name = "time",
        .init = time_init,
        .start = time_start,
        .stop = time_stop,
        .health_check = time_health_check,
        .critical = false,
        .auto_recover = true,
    };
    return &service;
}
