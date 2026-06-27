#include "smartwatch_app.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "smartwatch_board.h"
#include "smartwatch_platform.h"
#include "smartwatch_services.h"
#include "smartwatch_storage.h"
#include "smartwatch_ui.h"

static const char *TAG = "app";

static void app_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id != SMARTWATCH_EVENT_BUTTON) return;
    const smartwatch_input_event_t *event = data;
    if (event->input == SMARTWATCH_INPUT_POWER && event->action == SMARTWATCH_INPUT_LONG_PRESS) {
        ESP_LOGI(TAG, "Controlled shutdown requested");
        esp_err_t err = smartwatch_board_power_off();
        if (err != ESP_OK) ESP_LOGE(TAG, "Shutdown failed: %s", esp_err_to_name(err));
    }
}

esp_err_t smartwatch_app_start(void)
{
    ESP_RETURN_ON_ERROR(smartwatch_platform_init(), TAG, "Platform initialization failed");

    esp_err_t storage_err = smartwatch_storage_init();
    if (storage_err != ESP_OK) {
        ESP_LOGW(TAG, "Continuing without persistent storage: %s", esp_err_to_name(storage_err));
    }

    const smartwatch_board_config_t board_config = SMARTWATCH_BOARD_CONFIG_DEFAULT();
    smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_BOARD,
                                              SMARTWATCH_SERVICE_STARTING, ESP_OK);
    esp_err_t err = smartwatch_board_init(&board_config);
    if (err != ESP_OK) {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_BOARD,
                                                  SMARTWATCH_SERVICE_FAILED, err);
        return err;
    }
    smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_BOARD,
                                              SMARTWATCH_SERVICE_READY, ESP_OK);

    ESP_RETURN_ON_ERROR(smartwatch_ui_start(), TAG, "UI initialization failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(SMARTWATCH_EVENTS, SMARTWATCH_EVENT_BUTTON,
                                                   app_event_handler, NULL),
                        TAG, "App event registration failed");

    err = smartwatch_services_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "One or more services failed to start: %s", esp_err_to_name(err));
    }

    const uint8_t boot_marker = 1;
    smartwatch_platform_post(SMARTWATCH_EVENT_BOOT_COMPLETED, &boot_marker, sizeof(boot_marker), 100);
    ESP_LOGI(TAG, "SmartWatch OS started");
    return ESP_OK;
}
