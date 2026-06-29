#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*health_check)(void);
    bool critical;
    bool auto_recover;
} service_t;

typedef enum {
    SERVICE_STATE_STOPPED = 0,
    SERVICE_STATE_INITIALIZING,
    SERVICE_STATE_STARTING,
    SERVICE_STATE_READY,
    SERVICE_STATE_DEGRADED,
    SERVICE_STATE_FAILED,
} service_state_t;

typedef struct {
    const char *name;
    service_state_t state;
    esp_err_t last_error;
    uint8_t start_attempts;
    uint16_t failure_count;
    uint16_t restart_count;
    bool has_stop;
    bool critical;
    bool auto_recover;
    uint32_t next_retry_ms;
} service_runtime_info_t;

esp_err_t service_manager_init(void);
esp_err_t service_manager_register(const service_t *service);
esp_err_t service_manager_start_all(void);
size_t service_manager_count(void);
size_t service_manager_degraded_count(void);
size_t service_manager_failed_count(void);
const service_runtime_info_t *service_manager_info(size_t index);
const service_runtime_info_t *service_manager_find(const char *name);
esp_err_t service_manager_restart(const char *name);
