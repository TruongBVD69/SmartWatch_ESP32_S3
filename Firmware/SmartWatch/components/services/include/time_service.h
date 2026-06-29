#pragma once

#include "esp_err.h"
#include "platform_rtc.h"
#include "service_manager.h"

esp_err_t time_service_get(platform_datetime_t *datetime);
esp_err_t time_service_set(const platform_datetime_t *datetime);
const service_t *time_service_descriptor(void);
