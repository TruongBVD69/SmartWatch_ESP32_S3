#include "kernel.h"

#include "esp_check.h"
#include "app_manager.h"
#include "event_bus.h"
#include "input_manager.h"
#include "logger.h"
#include "screen_manager.h"
#include "session_manager.h"
#include "service_manager.h"
#include "storage_manager.h"

static const char *TAG = "kernel";

esp_err_t kernel_init(void)
{
    logger_init();
    ESP_RETURN_ON_ERROR(event_bus_init(), TAG, "Event bus initialization failed");
    ESP_RETURN_ON_ERROR(storage_manager_init(), TAG, "Storage initialization failed");
    ESP_RETURN_ON_ERROR(service_manager_init(), TAG, "Service manager initialization failed");
    ESP_RETURN_ON_ERROR(app_manager_init(), TAG, "App manager initialization failed");
    ESP_RETURN_ON_ERROR(screen_manager_init(), TAG, "Screen manager initialization failed");
    ESP_RETURN_ON_ERROR(session_manager_init(), TAG, "Session manager initialization failed");
    ESP_RETURN_ON_ERROR(input_manager_init(), TAG, "Input manager initialization failed");
    LOGI(TAG, "Kernel ready");
    return ESP_OK;
}
