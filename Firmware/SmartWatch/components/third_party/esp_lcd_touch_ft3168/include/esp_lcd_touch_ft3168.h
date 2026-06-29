#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_LCD_TOUCH_IO_I2C_FT3168_ADDRESS 0x38

#define ESP_LCD_TOUCH_IO_I2C_FT3168_CONFIG()             \
    {                                                    \
        .scl_speed_hz = 400000,                          \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT3168_ADDRESS, \
        .control_phase_bytes = 1,                        \
        .dc_bit_offset = 0,                              \
        .lcd_cmd_bits = 8,                               \
        .flags = { .disable_control_phase = 1 },         \
    }

esp_err_t esp_lcd_touch_new_i2c_ft3168(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t *config,
                                       esp_lcd_touch_handle_t *out_touch);

#ifdef __cplusplus
}
#endif
