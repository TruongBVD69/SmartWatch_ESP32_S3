#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "service_manager.h"

esp_err_t storage_service_mount_sd(void);
esp_err_t storage_service_unmount_sd(void);
bool storage_service_sd_is_mounted(void);
const service_t *storage_service_descriptor(void);
