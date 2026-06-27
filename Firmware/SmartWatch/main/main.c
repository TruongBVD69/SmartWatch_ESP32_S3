#include "smartwatch_app.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t err = smartwatch_app_start();
    if (err == ESP_OK) return;

    ESP_LOGE(TAG, "Fatal startup error: %s; restarting in 5 seconds", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
