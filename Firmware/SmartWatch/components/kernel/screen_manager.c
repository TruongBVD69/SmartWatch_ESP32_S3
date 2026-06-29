#include "screen_manager.h"

#include "platform_display.h"

static lv_obj_t *current;

esp_err_t screen_manager_init(void)
{
    current = NULL;
    return platform_display_is_ready() ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t screen_manager_show(lv_obj_t *screen)
{
    if (screen == NULL) return ESP_ERR_INVALID_ARG;
    if (!platform_display_lock(500)) return ESP_ERR_TIMEOUT;
    lv_screen_load(screen);
    current = screen;
    platform_display_unlock();
    return ESP_OK;
}

lv_obj_t *screen_manager_current(void) { return current; }
