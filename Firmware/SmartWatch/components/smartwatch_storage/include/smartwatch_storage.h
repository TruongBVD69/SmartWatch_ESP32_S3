#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool nvs_ready;
    bool assets_ready;
} smartwatch_storage_status_t;

esp_err_t smartwatch_storage_init(void);
const smartwatch_storage_status_t *smartwatch_storage_get_status(void);
esp_err_t smartwatch_storage_get_u32(const char *key, uint32_t *value);
esp_err_t smartwatch_storage_set_u32(const char *key, uint32_t value);

#ifdef __cplusplus
}
#endif
