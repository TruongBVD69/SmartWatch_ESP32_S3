#include "platform_power.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "smartwatch_board.h"

static const char *TAG = "platform_power";
static const uint64_t GPIO0_MASK = (1ULL << GPIO_NUM_0);
static platform_wakeup_cause_t wakeup_cause = {
    .reason = PLATFORM_WAKE_REASON_COLD_BOOT,
};

static platform_wake_reason_t classify_wakeup_reason(uint32_t causes_bitmap,
                                                     uint64_t ext1_mask)
{
    if (causes_bitmap & BIT(ESP_SLEEP_WAKEUP_EXT0)) {
        return PLATFORM_WAKE_REASON_POWER_BUTTON;
    }
    if (causes_bitmap & BIT(ESP_SLEEP_WAKEUP_EXT1)) {
        if (ext1_mask & GPIO0_MASK) {
            return PLATFORM_WAKE_REASON_BOOT_BUTTON;
        }
        return PLATFORM_WAKE_REASON_UNKNOWN;
    }
    if (causes_bitmap & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
        return PLATFORM_WAKE_REASON_TIMER;
    }
    if (causes_bitmap & BIT(ESP_SLEEP_WAKEUP_UNDEFINED)) {
        return PLATFORM_WAKE_REASON_COLD_BOOT;
    }
    return PLATFORM_WAKE_REASON_UNKNOWN;
}

void platform_power_init_boot_context(void)
{
    const uint32_t causes_bitmap = esp_sleep_get_wakeup_causes();
    const uint64_t ext1_mask = esp_sleep_get_ext1_wakeup_status();

    wakeup_cause = (platform_wakeup_cause_t) {
        .reason = classify_wakeup_reason(causes_bitmap, ext1_mask),
        .from_sleep = !(causes_bitmap & BIT(ESP_SLEEP_WAKEUP_UNDEFINED)),
        .causes_bitmap = causes_bitmap,
        .ext1_wakeup_mask = ext1_mask,
    };

    ESP_LOGI(TAG, "Wake cause=%d bitmap=0x%lx ext1=0x%llx",
             wakeup_cause.reason,
             (unsigned long)wakeup_cause.causes_bitmap,
             (unsigned long long)wakeup_cause.ext1_wakeup_mask);
}

platform_wakeup_cause_t platform_power_get_wakeup_cause(void)
{
    return wakeup_cause;
}

esp_err_t platform_power_get_status(platform_power_status_t *status)
{
    if (status == NULL) return ESP_ERR_INVALID_ARG;
    smartwatch_board_power_status_t board = {0};
    esp_err_t err = smartwatch_board_power_get_status(&board);
    if (err != ESP_OK) return err;
    *status = (platform_power_status_t) {
        .battery_present = board.battery_present,
        .vbus_present = board.vbus_present,
        .charging = board.charging,
        .battery_percent = board.battery_percent,
        .battery_mv = board.battery_mv,
        .temperature_c = board.pmic_temperature_c,
    };
    return ESP_OK;
}

esp_err_t platform_power_get_event(platform_power_event_t *event, uint32_t timeout_ms)
{
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    smartwatch_board_power_event_t board_event = {0};
    esp_err_t err = smartwatch_board_power_get_event(&board_event, timeout_ms);
    if (err != ESP_OK) {
        return err;
    }

    event->type = (platform_power_event_type_t)board_event.type;
    event->status = (platform_power_status_t) {
        .battery_present = board_event.status.battery_present,
        .vbus_present = board_event.status.vbus_present,
        .charging = board_event.status.charging,
        .battery_percent = board_event.status.battery_percent,
        .battery_mv = board_event.status.battery_mv,
        .temperature_c = board_event.status.pmic_temperature_c,
    };
    return ESP_OK;
}

esp_err_t platform_power_off(void)
{
    return smartwatch_board_power_off();
}

esp_err_t platform_power_enter_deep_sleep(uint32_t wake_timer_ms)
{
    const uint64_t ext1_mask = smartwatch_board_get_wakeup_ext1_mask();
    const gpio_num_t ext0_gpio = smartwatch_board_get_wakeup_ext0_gpio();
    const int ext0_level = smartwatch_board_get_wakeup_ext0_level();

    (void)smartwatch_board_display_set_backlight(false);
    (void)smartwatch_board_audio_deinit();

    ESP_RETURN_ON_ERROR(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL),
                        TAG, "clear wake sources failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_ext0_wakeup(ext0_gpio, ext0_level),
                        TAG, "enable ext0 wake failed");
    ESP_RETURN_ON_ERROR(esp_sleep_disable_ext1_wakeup_io(0), TAG, "clear ext1 wake failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_ext1_wakeup_io(ext1_mask, ESP_EXT1_WAKEUP_ANY_LOW),
                        TAG, "enable ext1 wake failed");

    if (wake_timer_ms > 0) {
        ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup((uint64_t)wake_timer_ms * 1000ULL),
                            TAG, "enable timer wake failed");
    }

    ESP_LOGI(TAG, "Entering deep sleep");
    esp_deep_sleep_start();
    return ESP_OK;
}
