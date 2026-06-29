#include "ui_theme.h"

void ui_theme_init(void) {}

void ui_theme_apply_screen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x050708), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

void ui_theme_apply_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_all(obj, 12, 0);
}

lv_color_t ui_theme_text_primary(void) { return lv_color_hex(0xF1F4F6); }
lv_color_t ui_theme_text_secondary(void) { return lv_color_hex(0xA8B1B7); }
lv_color_t ui_theme_accent(void) { return lv_color_hex(0x38BDF8); }
lv_color_t ui_theme_surface(void) { return lv_color_hex(0x10161C); }
lv_color_t ui_theme_success(void) { return lv_color_hex(0x22C55E); }
lv_color_t ui_theme_warning(void) { return lv_color_hex(0xF59E0B); }
lv_color_t ui_theme_danger(void) { return lv_color_hex(0xEF4444); }
