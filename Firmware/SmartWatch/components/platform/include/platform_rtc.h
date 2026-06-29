#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} platform_datetime_t;

esp_err_t platform_rtc_get(platform_datetime_t *datetime);
esp_err_t platform_rtc_set(const platform_datetime_t *datetime);
