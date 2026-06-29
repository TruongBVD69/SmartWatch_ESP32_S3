#include "apps.h"

#include "app_manager.h"
#include "diagnostics_app.h"
#include "launcher_app.h"
#include "settings_app.h"
#include "watchface_app.h"

esp_err_t apps_init(void)
{
    const app_t *apps[] = {
        watchface_app_descriptor(), launcher_app_descriptor(),
        settings_app_descriptor(), diagnostics_app_descriptor(),
    };
    for (size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); ++i) {
        esp_err_t err = app_manager_register(apps[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}
