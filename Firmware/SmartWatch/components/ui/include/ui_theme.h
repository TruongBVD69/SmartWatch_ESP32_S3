#pragma once

#include "lvgl.h"

void ui_theme_init(void);
void ui_theme_apply_screen(lv_obj_t *screen);
void ui_theme_apply_panel(lv_obj_t *obj);
lv_color_t ui_theme_text_primary(void);
lv_color_t ui_theme_text_secondary(void);
lv_color_t ui_theme_accent(void);
lv_color_t ui_theme_surface(void);
lv_color_t ui_theme_success(void);
lv_color_t ui_theme_warning(void);
lv_color_t ui_theme_danger(void);
