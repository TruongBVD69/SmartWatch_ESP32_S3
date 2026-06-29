#include "platform.h"

#include "platform_power.h"
#include "smartwatch_board.h"

static platform_status_t status;

esp_err_t platform_init(void)
{
    platform_power_init_boot_context();

    const smartwatch_board_config_t config = SMARTWATCH_BOARD_CONFIG_DEFAULT();
    esp_err_t err = smartwatch_board_init(&config);
    if (err != ESP_OK) return err;

    const smartwatch_board_status_t *board = smartwatch_board_get_status();
    status = (platform_status_t) {
        .display_ready = board->display_ready,
        .touch_ready = board->touch_ready,
        .imu_ready = board->imu_ready,
        .rtc_ready = board->rtc_ready,
        .power_ready = board->power_ready,
        .buttons_ready = board->buttons_ready,
    };
    return ESP_OK;
}

const platform_status_t *platform_get_status(void)
{
    return &status;
}
