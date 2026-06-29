#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    bool nvs_ready;
    bool assets_ready;
} storage_manager_status_t;

esp_err_t storage_manager_init(void);
const storage_manager_status_t *storage_manager_status(void);
esp_err_t storage_manager_get_u32(const char *key, uint32_t *value);
esp_err_t storage_manager_set_u32(const char *key, uint32_t value);
