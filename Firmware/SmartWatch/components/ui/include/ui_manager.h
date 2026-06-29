#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

esp_err_t ui_init(void);
bool ui_manager_lock(uint32_t timeout_ms);
void ui_manager_unlock(void);
lv_obj_t *ui_manager_create_screen(void);
