#pragma once

#include "app_manager.h"
#include "esp_err.h"

esp_err_t ui_events_open_app(app_id_t app_id);
esp_err_t ui_events_back(void);
