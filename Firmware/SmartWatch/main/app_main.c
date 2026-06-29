#include "app_manager.h"
#include "apps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "kernel.h"
#include "platform.h"
#include "services.h"
#include "ui_manager.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t err = platform_init();
    if (err == ESP_OK) err = kernel_init();
    if (err == ESP_OK) err = services_init();
    if (err == ESP_OK) err = ui_init();
    if (err == ESP_OK) err = apps_init();
    if (err == ESP_OK) err = app_manager_start(APP_ID_WATCHFACE);
    if (err == ESP_OK) return;

    ESP_LOGE(TAG, "Startup failed: %s; restarting in 5 seconds", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
