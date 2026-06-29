#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool battery_present;
    bool vbus_present;
    bool charging;
    int battery_percent;
    uint16_t battery_mv;
    float temperature_c;
} platform_power_status_t;

typedef enum {
    PLATFORM_POWER_EVENT_BATTERY_INSERTED = 0,
    PLATFORM_POWER_EVENT_BATTERY_REMOVED,
    PLATFORM_POWER_EVENT_VBUS_INSERTED,
    PLATFORM_POWER_EVENT_VBUS_REMOVED,
    PLATFORM_POWER_EVENT_CHARGE_STARTED,
    PLATFORM_POWER_EVENT_CHARGE_STOPPED,
    PLATFORM_POWER_EVENT_CHARGE_COMPLETED,
} platform_power_event_type_t;

typedef struct {
    platform_power_event_type_t type;
    platform_power_status_t status;
} platform_power_event_t;

typedef enum {
    PLATFORM_WAKE_REASON_COLD_BOOT = 0,
    PLATFORM_WAKE_REASON_BOOT_BUTTON,
    PLATFORM_WAKE_REASON_POWER_BUTTON,
    PLATFORM_WAKE_REASON_TOUCH,
    PLATFORM_WAKE_REASON_TIMER,
    PLATFORM_WAKE_REASON_UNKNOWN,
} platform_wake_reason_t;

typedef struct {
    platform_wake_reason_t reason;
    bool from_sleep;
    uint32_t causes_bitmap;
    uint64_t ext1_wakeup_mask;
} platform_wakeup_cause_t;

void platform_power_init_boot_context(void);
platform_wakeup_cause_t platform_power_get_wakeup_cause(void);
esp_err_t platform_power_get_status(platform_power_status_t *status);
esp_err_t platform_power_get_event(platform_power_event_t *event, uint32_t timeout_ms);
esp_err_t platform_power_off(void);
esp_err_t platform_power_enter_deep_sleep(uint32_t wake_timer_ms);
