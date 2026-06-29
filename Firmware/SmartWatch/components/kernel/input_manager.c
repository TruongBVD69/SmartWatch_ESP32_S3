#include "input_manager.h"

#include "app_manager.h"
#include "esp_task_wdt.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"
#include "platform_input.h"
#include "session_manager.h"

static const char *TAG = "input";

static void route_input(const platform_input_event_t *input)
{
    if (input->action == PLATFORM_INPUT_CLICK) {
        if (input->input == PLATFORM_INPUT_BOOT) {
            if (app_manager_current() != NULL &&
                app_manager_current()->id == APP_ID_WATCHFACE) {
                app_manager_open(APP_ID_LAUNCHER, NULL);
            } else if (app_manager_can_go_back()) {
                app_manager_back();
            } else {
                app_manager_open_root(APP_ID_WATCHFACE, NULL);
            }
        } else if (input->input == PLATFORM_INPUT_POWER) {
            if (app_manager_can_go_back()) {
                app_manager_back();
            } else {
                app_manager_open_root(APP_ID_WATCHFACE, NULL);
            }
        }
    }

    if (input->input == PLATFORM_INPUT_BOOT &&
        input->action == PLATFORM_INPUT_LONG_PRESS) {
        app_manager_open(APP_ID_DIAGNOSTICS, NULL);
    }
}

static void input_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);
    while (true) {
        platform_input_event_t input;
        esp_err_t err = platform_input_get_event(&input, 1000);
        if (err == ESP_OK) {
            const event_input_t event = {
                .input = input.input,
                .action = input.action,
                .duration_ms = input.duration_ms,
            };
            event_bus_publish(EVENT_UI_ACTION, &event, sizeof(event), 100);
            bool consumed = session_manager_handle_input(&input);
            if (!consumed) {
                route_input(&input);
            }
        }
        esp_task_wdt_reset();
    }
}

esp_err_t input_manager_init(void)
{
    return xTaskCreate(input_task, "input_manager", 3072, NULL, 5, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}
