#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_task_wdt.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(SMARTWATCH_EVENTS);

typedef enum {
    SMARTWATCH_EVENT_BOOT_COMPLETED,
    SMARTWATCH_EVENT_SERVICE_STATE,
    SMARTWATCH_EVENT_POWER_STATUS,
    SMARTWATCH_EVENT_POWER_CHANGED,
    SMARTWATCH_EVENT_BUTTON,
    SMARTWATCH_EVENT_RTC_TICK,
    SMARTWATCH_EVENT_STORAGE_STATE,
    SMARTWATCH_EVENT_SYSTEM_HEALTH,
} smartwatch_event_id_t;

typedef enum {
    SMARTWATCH_SERVICE_PLATFORM,
    SMARTWATCH_SERVICE_STORAGE,
    SMARTWATCH_SERVICE_BOARD,
    SMARTWATCH_SERVICE_POWER,
    SMARTWATCH_SERVICE_INPUT,
    SMARTWATCH_SERVICE_RTC,
    SMARTWATCH_SERVICE_HEALTH,
    SMARTWATCH_SERVICE_UI,
} smartwatch_service_id_t;

typedef enum {
    SMARTWATCH_SERVICE_STARTING,
    SMARTWATCH_SERVICE_READY,
    SMARTWATCH_SERVICE_DEGRADED,
    SMARTWATCH_SERVICE_FAILED,
} smartwatch_service_state_t;

typedef struct {
    smartwatch_service_id_t service;
    smartwatch_service_state_t state;
    esp_err_t error;
} smartwatch_service_event_t;

typedef struct {
    bool nvs_ready;
    bool assets_ready;
} smartwatch_storage_event_t;

typedef struct {
    bool battery_present;
    bool vbus_present;
    bool charging;
    int battery_percent;
    uint16_t battery_mv;
    float pmic_temperature_c;
} smartwatch_power_event_t;

typedef enum {
    SMARTWATCH_INPUT_BOOT,
    SMARTWATCH_INPUT_POWER,
} smartwatch_input_id_t;

typedef enum {
    SMARTWATCH_INPUT_PRESS,
    SMARTWATCH_INPUT_RELEASE,
    SMARTWATCH_INPUT_CLICK,
    SMARTWATCH_INPUT_LONG_PRESS,
} smartwatch_input_action_t;

typedef struct {
    smartwatch_input_id_t input;
    smartwatch_input_action_t action;
    uint32_t duration_ms;
} smartwatch_input_event_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} smartwatch_rtc_event_t;

typedef struct {
    uint32_t free_heap;
    uint32_t minimum_free_heap;
    uint32_t free_psram;
} smartwatch_health_event_t;

esp_err_t smartwatch_platform_init(void);
esp_err_t smartwatch_platform_post(int32_t event_id, const void *data, size_t data_size,
                                   uint32_t timeout_ms);
esp_err_t smartwatch_platform_publish_service_state(smartwatch_service_id_t service,
                                                    smartwatch_service_state_t state,
                                                    esp_err_t error);
const char *smartwatch_platform_service_name(smartwatch_service_id_t service);
const char *smartwatch_platform_service_state_name(smartwatch_service_state_t state);

esp_err_t smartwatch_platform_watchdog_subscribe(void);
esp_err_t smartwatch_platform_watchdog_feed(void);
void smartwatch_platform_watchdog_unsubscribe(void);

#ifdef __cplusplus
}
#endif
