#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "platform_input.h"

typedef enum {
    SESSION_STATE_ACTIVE = 0,
    SESSION_STATE_IDLE,
    SESSION_STATE_DIM,
    SESSION_STATE_SLEEP_REQUESTED,
} session_state_t;

typedef enum {
    SESSION_REASON_BOOT = 0,
    SESSION_REASON_INPUT,
    SESSION_REASON_TIMEOUT,
    SESSION_REASON_POWER_EVENT,
    SESSION_REASON_POWER_BUTTON,
} session_reason_t;

typedef struct {
    session_state_t state;
    session_reason_t reason;
    uint8_t brightness;
    uint32_t idle_ms;
} session_status_t;

typedef struct {
    uint32_t idle_timeout_ms;
    uint32_t dim_timeout_ms;
    uint32_t sleep_timeout_ms;
    uint32_t deep_sleep_timeout_ms;
    uint8_t active_brightness;
    uint8_t dim_brightness;
} session_config_t;

esp_err_t session_manager_init(void);
void session_manager_note_activity(session_reason_t reason);
bool session_manager_handle_input(const platform_input_event_t *input);
esp_err_t session_manager_request_power_off(void);
session_status_t session_manager_get_status(void);
session_config_t session_manager_get_config(void);
esp_err_t session_manager_set_config(const session_config_t *config);
