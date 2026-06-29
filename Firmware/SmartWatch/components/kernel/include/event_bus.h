#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(SMARTWATCH_EVENTS);

typedef enum {
    EVENT_SYSTEM_READY,
    EVENT_SERVICE_STATE,
    EVENT_APP_STATE,
    EVENT_TIME_UPDATED,
    EVENT_BATTERY_UPDATED,
    EVENT_SENSOR_UPDATED,
    EVENT_UI_ACTION,
    EVENT_SESSION_STATE,
} event_id_t;

typedef enum {
    EVENT_SERVICE_STATE_STOPPED = 0,
    EVENT_SERVICE_STATE_INITIALIZING,
    EVENT_SERVICE_STATE_STARTING,
    EVENT_SERVICE_STATE_READY,
    EVENT_SERVICE_STATE_DEGRADED,
    EVENT_SERVICE_STATE_FAILED,
} event_service_state_kind_t;

typedef enum {
    EVENT_APP_STATE_ENTERED = 0,
    EVENT_APP_STATE_EXITED,
    EVENT_APP_STATE_PAUSED,
    EVENT_APP_STATE_RESUMED,
} event_app_state_kind_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} event_time_t;

typedef struct {
    bool present;
    bool charging;
    int percent;
    uint16_t millivolts;
} event_battery_t;

typedef struct {
    float temperature_c;
    float accel_mag;
    float gyro_mag;
    uint32_t step_count;
    uint16_t cadence_spm;
    uint8_t step_confidence;
    bool walking;
} event_sensor_t;

typedef struct {
    const char *name;
    uint8_t state;
    int32_t last_error;
    uint16_t failures;
    uint16_t restarts;
} event_service_t;

typedef struct {
    int32_t app_id;
    uint8_t state;
} event_app_t;

typedef struct {
    uint8_t input;
    uint8_t action;
    uint32_t duration_ms;
} event_input_t;

typedef struct {
    uint8_t state;
    uint8_t reason;
    uint8_t brightness;
    uint32_t idle_ms;
} event_session_t;

typedef void (*event_bus_handler_t)(void *arg, esp_event_base_t base, int32_t id, void *data);

esp_err_t event_bus_init(void);
esp_err_t event_bus_publish(event_id_t id, const void *data, size_t size, uint32_t timeout_ms);
esp_err_t event_bus_subscribe(event_id_t id, event_bus_handler_t handler, void *arg);
esp_err_t event_bus_unsubscribe(event_id_t id, event_bus_handler_t handler);
