#include "launcher_app.h"

#include "battery_service.h"
#include "service_manager.h"
#include "session_manager.h"
#include "screen_manager.h"
#include "ui_manager.h"
#include "ui_squareline.h"
#include "ui_theme.h"

static lv_obj_t *screen;
static lv_obj_t *status_label;

static lv_obj_t *create_tile(lv_obj_t *parent, const char *title, const char *subtitle,
                             lv_coord_t col, lv_coord_t row)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 156, 92);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, col, row);

    lv_obj_t *title_label = lv_label_create(button);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, ui_theme_text_primary(), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 4);

    lv_obj_t *subtitle_label = lv_label_create(button);
    lv_obj_set_width(subtitle_label, 120);
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(subtitle_label, subtitle);
    lv_obj_set_style_text_color(subtitle_label, ui_theme_text_secondary(), 0);
    lv_obj_align(subtitle_label, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    return button;
}

static void launcher_refresh_status(void)
{
    if (status_label == NULL) {
        return;
    }
    platform_power_status_t power = {0};
    session_status_t session = session_manager_get_status();
    const bool battery_ok = battery_service_get(&power) == ESP_OK;
    const size_t degraded = service_manager_degraded_count();
    const size_t failed = service_manager_failed_count();
    const char *battery_state = battery_ok ? (power.charging ? "charging" : "ready") : "off";
    const char *session_state = "active";
    switch (session.state) {
    case SESSION_STATE_IDLE: session_state = "idle"; break;
    case SESSION_STATE_DIM: session_state = "dim"; break;
    case SESSION_STATE_SLEEP_REQUESTED: session_state = "sleep"; break;
    case SESSION_STATE_ACTIVE: default: session_state = "active"; break;
    }

    if (!ui_manager_lock(200)) {
        return;
    }
    lv_label_set_text_fmt(status_label,
                          "Battery %s  Session %s  Services %u/%u",
                          battery_state,
                          session_state,
                          (unsigned)degraded,
                          (unsigned)failed);
    lv_obj_set_style_text_color(status_label,
                                failed > 0 ? ui_theme_danger() :
                                degraded > 0 ? ui_theme_warning() :
                                ui_theme_text_secondary(), 0);
    ui_manager_unlock();
}

static esp_err_t launcher_init(void)
{
    if (!ui_manager_lock(1000)) return ESP_ERR_TIMEOUT;
    if (ui_squareline_available()) {
        screen = ui_squareline_create_screen(UI_SQUARELINE_SCREEN_LAUNCHER);
        if (screen != NULL) {
            status_label = NULL;
            ui_squareline_launcher_setup();
            ui_manager_unlock();
            return ESP_OK;
        }
    }

    screen = ui_manager_create_screen();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Launcher");
    lv_obj_set_style_text_color(title, ui_theme_text_primary(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 24, 22);

    status_label = lv_label_create(screen);
    lv_obj_set_width(status_label, 320);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(status_label, ui_theme_text_secondary(), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_LEFT, 24, 48);

    lv_obj_t *watchface_tile = create_tile(screen, "WatchFace", "Home screen and live status", 24, 86);
    lv_obj_t *settings_tile = create_tile(screen, "Settings", "Display, power and services", 190, 86);
    lv_obj_t *diagnostics_tile = create_tile(screen, "Diagnostics", "Runtime tests and board validation", 24, 190);
    lv_obj_t *health_tile = create_tile(screen, "Service Health", "Inspect recovery states in Settings", 190, 190);

    ui_squareline_bind_open_app(watchface_tile, APP_ID_WATCHFACE);
    ui_squareline_bind_open_app(settings_tile, APP_ID_SETTINGS);
    ui_squareline_bind_open_app(diagnostics_tile, APP_ID_DIAGNOSTICS);
    ui_squareline_bind_open_app(health_tile, APP_ID_SETTINGS);
    ui_manager_unlock();
    return ESP_OK;
}

static esp_err_t launcher_enter(void *param)
{
    (void)param;
    launcher_refresh_status();
    return screen_manager_show(screen);
}

const app_t *launcher_app_descriptor(void)
{
    static const app_t app = {
        .id = APP_ID_LAUNCHER, .name = "Launcher", .init = launcher_init, .enter = launcher_enter,
    };
    return &app;
}
