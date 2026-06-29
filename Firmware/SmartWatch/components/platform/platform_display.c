#include "platform_display.h"

#include "platform.h"
#include "smartwatch_board.h"

bool platform_display_is_ready(void)
{
    return platform_get_status()->display_ready;
}

lv_display_t *platform_display_get(void)
{
    return smartwatch_board_get_display();
}

bool platform_display_lock(uint32_t timeout_ms)
{
    return smartwatch_board_display_lock(timeout_ms);
}

void platform_display_unlock(void)
{
    smartwatch_board_display_unlock();
}

esp_err_t platform_display_set_brightness(uint8_t percent)
{
    if (!platform_display_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!platform_display_lock(500)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = smartwatch_board_display_set_brightness(percent);
    platform_display_unlock();
    return err;
}

uint8_t platform_display_get_brightness(void)
{
    return smartwatch_board_display_get_brightness();
}

esp_err_t platform_display_set_backlight(bool enabled)
{
    if (!platform_display_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!platform_display_lock(500)) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = smartwatch_board_display_set_backlight(enabled);
    platform_display_unlock();
    return err;
}
