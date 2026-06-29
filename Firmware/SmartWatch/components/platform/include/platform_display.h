#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

bool platform_display_is_ready(void);
lv_display_t *platform_display_get(void);
bool platform_display_lock(uint32_t timeout_ms);
void platform_display_unlock(void);
esp_err_t platform_display_set_brightness(uint8_t percent);
uint8_t platform_display_get_brightness(void);
esp_err_t platform_display_set_backlight(bool enabled);
