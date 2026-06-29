#pragma once

#include <stdbool.h>

#include "app_manager.h"
#include "esp_err.h"
#include "lvgl.h"

typedef enum {
    UI_SQUARELINE_SCREEN_WATCHFACE = 0,
    UI_SQUARELINE_SCREEN_LAUNCHER,
    UI_SQUARELINE_SCREEN_SETTINGS,
    UI_SQUARELINE_SCREEN_DIAGNOSTICS,
} ui_squareline_screen_id_t;

esp_err_t ui_squareline_init(void);
bool ui_squareline_available(void);
lv_obj_t *ui_squareline_create_screen(ui_squareline_screen_id_t screen_id);
bool ui_squareline_watchface_bind(lv_obj_t **time_label, lv_obj_t **date_label,
                                  lv_obj_t **battery_label);
void ui_squareline_watchface_set_time(unsigned hour, unsigned minute, unsigned day,
                                      unsigned month, unsigned year);
void ui_squareline_watchface_set_battery(int percent, bool charging);
void ui_squareline_watchface_set_temperature(float temperature_c);
void ui_squareline_watchface_set_steps(uint32_t step_count);
void ui_squareline_watchface_set_acceleration(float accel_mag);
void ui_squareline_launcher_setup(void);
void ui_squareline_bind_open_app(lv_obj_t *obj, app_id_t app_id);
void ui_squareline_bind_back(lv_obj_t *obj);
