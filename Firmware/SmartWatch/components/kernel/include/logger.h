#pragma once

#include "esp_log.h"

#define LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)

void logger_init(void);
