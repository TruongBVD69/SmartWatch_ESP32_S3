#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef enum {
    PLATFORM_INPUT_BOOT,
    PLATFORM_INPUT_POWER,
} platform_input_id_t;

typedef enum {
    PLATFORM_INPUT_PRESS,
    PLATFORM_INPUT_RELEASE,
    PLATFORM_INPUT_CLICK,
    PLATFORM_INPUT_LONG_PRESS,
} platform_input_action_t;

typedef struct {
    platform_input_id_t input;
    platform_input_action_t action;
    uint32_t duration_ms;
} platform_input_event_t;

esp_err_t platform_input_get_event(platform_input_event_t *event, uint32_t timeout_ms);
