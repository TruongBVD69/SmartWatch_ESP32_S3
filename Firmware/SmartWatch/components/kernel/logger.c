#include "logger.h"

void logger_init(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    esp_log_level_set("httpd", ESP_LOG_WARN);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_WARN);
    esp_log_level_set("outbox", ESP_LOG_WARN);
    esp_log_level_set("SD_HOST", ESP_LOG_ERROR);
}
