#include "ui_events.h"

esp_err_t ui_events_open_app(app_id_t app_id)
{
    return app_manager_open(app_id, NULL);
}

esp_err_t ui_events_back(void)
{
    return app_manager_back();
}
