#include "platform_input.h"

#include "smartwatch_board.h"

esp_err_t platform_input_get_event(platform_input_event_t *event, uint32_t timeout_ms)
{
    if (event == NULL) return ESP_ERR_INVALID_ARG;
    smartwatch_board_button_event_t board;
    esp_err_t err = smartwatch_board_button_get_event(&board, timeout_ms);
    if (err != ESP_OK) return err;
    *event = (platform_input_event_t) {
        .input = board.button == SMARTWATCH_BUTTON_POWER ? PLATFORM_INPUT_POWER : PLATFORM_INPUT_BOOT,
        .action = (platform_input_action_t)board.type,
        .duration_ms = board.duration_ms,
    };
    return ESP_OK;
}
