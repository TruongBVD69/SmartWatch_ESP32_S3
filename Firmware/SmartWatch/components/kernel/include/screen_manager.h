#pragma once

#include "esp_err.h"
#include "lvgl.h"

esp_err_t screen_manager_init(void);
esp_err_t screen_manager_show(lv_obj_t *screen);
lv_obj_t *screen_manager_current(void);
