#pragma once

#include "esp_err.h"
#include "platform_power.h"
#include "service_manager.h"

esp_err_t battery_service_get(platform_power_status_t *status);
const service_t *battery_service_descriptor(void);
