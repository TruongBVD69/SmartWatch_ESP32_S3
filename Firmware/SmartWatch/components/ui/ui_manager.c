#include "ui_manager.h"

#include "platform_display.h"
#include "ui_squareline.h"
#include "ui_theme.h"

esp_err_t ui_init(void)
{
    if (!platform_display_is_ready()) return ESP_ERR_INVALID_STATE;
    if (!platform_display_lock(1000)) return ESP_ERR_TIMEOUT;
    ui_theme_init();
    esp_err_t err = ui_squareline_init();
    platform_display_unlock();
    return err;
}

bool ui_manager_lock(uint32_t timeout_ms) { return platform_display_lock(timeout_ms); }
void ui_manager_unlock(void) { platform_display_unlock(); }

lv_obj_t *ui_manager_create_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    ui_theme_apply_screen(screen);
    return screen;
}
