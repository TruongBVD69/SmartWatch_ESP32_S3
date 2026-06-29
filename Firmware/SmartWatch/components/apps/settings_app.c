#include "settings_app.h"

#include "app_manager.h"
#include "battery_service.h"
#include "event_bus.h"
#include "session_manager.h"
#include "service_manager.h"
#include "screen_manager.h"
#include "time_service.h"
#include "ui_manager.h"
#include "ui_theme.h"

static lv_obj_t *screen;
static lv_obj_t *overview_label;
static lv_obj_t *display_label;
static lv_obj_t *services_label;
static lv_obj_t *preset_label;
static lv_obj_t *action_label;
static bool settings_visible;

static const session_config_t preset_short = {
    .idle_timeout_ms = 10000,
    .dim_timeout_ms = 20000,
    .sleep_timeout_ms = 30000,
    .deep_sleep_timeout_ms = 60000,
    .active_brightness = 100,
    .dim_brightness = 30,
};

static const session_config_t preset_normal = {
    .idle_timeout_ms = 15000,
    .dim_timeout_ms = 30000,
    .sleep_timeout_ms = 45000,
    .deep_sleep_timeout_ms = 180000,
    .active_brightness = 100,
    .dim_brightness = 30,
};

static const session_config_t preset_long = {
    .idle_timeout_ms = 30000,
    .dim_timeout_ms = 60000,
    .sleep_timeout_ms = 90000,
    .deep_sleep_timeout_ms = 600000,
    .active_brightness = 100,
    .dim_brightness = 30,
};

static const char *service_state_name(service_state_t state)
{
    switch (state) {
    case SERVICE_STATE_STOPPED: return "STOPPED";
    case SERVICE_STATE_INITIALIZING: return "INIT";
    case SERVICE_STATE_STARTING: return "START";
    case SERVICE_STATE_READY: return "READY";
    case SERVICE_STATE_DEGRADED: return "DEGRADED";
    case SERVICE_STATE_FAILED: return "FAILED";
    }
    return "?";
}

static const char *session_state_name(session_state_t state)
{
    switch (state) {
    case SESSION_STATE_ACTIVE: return "ACTIVE";
    case SESSION_STATE_IDLE: return "IDLE";
    case SESSION_STATE_DIM: return "DIM";
    case SESSION_STATE_SLEEP_REQUESTED: return "SLEEP_REQ";
    }
    return "?";
}

static const char *session_reason_name(session_reason_t reason)
{
    switch (reason) {
    case SESSION_REASON_BOOT: return "BOOT";
    case SESSION_REASON_INPUT: return "INPUT";
    case SESSION_REASON_TIMEOUT: return "TIMEOUT";
    case SESSION_REASON_POWER_EVENT: return "POWER_EVT";
    case SESSION_REASON_POWER_BUTTON: return "POWER_BTN";
    }
    return "?";
}

static void settings_refresh(void)
{
    platform_datetime_t datetime = {0};
    platform_power_status_t power = {0};
    session_status_t session_status = session_manager_get_status();
    session_config_t session = session_manager_get_config();
    const bool time_ok = time_service_get(&datetime) == ESP_OK;
    const bool power_ok = battery_service_get(&power) == ESP_OK;
    char services_text[256] = {0};
    size_t used = 0;

    for (size_t i = 0; i < service_manager_count() && used < sizeof(services_text) - 1; ++i) {
        const service_runtime_info_t *info = service_manager_info(i);
        if (info == NULL) {
            continue;
        }
        int written = snprintf(services_text + used, sizeof(services_text) - used,
                               "%s=%s f=%u r=%u\n",
                               info->name, service_state_name(info->state),
                               info->failure_count, info->restart_count);
        if (written < 0) {
            break;
        }
        size_t next = used + (size_t)written;
        used = next < sizeof(services_text) ? next : sizeof(services_text) - 1;
    }

    if (ui_manager_lock(300)) {
        lv_obj_set_style_text_color(
            action_label,
            service_manager_failed_count() > 0 ? ui_theme_danger() :
            service_manager_degraded_count() > 0 ? ui_theme_warning() :
            ui_theme_text_secondary(),
            0);
        lv_label_set_text_fmt(
            overview_label,
            "App: %s\n"
            "Back stack: %s (%u)\n"
            "RTC: %s\n"
            "Battery: %s\n"
            "Session: %s via %s\n"
            "Issues: %u degraded / %u failed",
            app_manager_current() != NULL ? app_manager_current()->name : "-",
            app_manager_can_go_back() ? "available" : "empty",
            (unsigned)app_manager_history_depth(),
            time_ok ? "ready" : "unavailable",
            power_ok ? "ready" : "unavailable",
            session_state_name(session_status.state),
            session_reason_name(session_status.reason),
            (unsigned)service_manager_degraded_count(),
            (unsigned)service_manager_failed_count());
        lv_label_set_text_fmt(
            display_label,
            "Idle %lus\n"
            "Dim %lus\n"
            "Sleep %lus\n"
            "Deep %lum\n"
            "Active %u%%\n"
            "Dim %u%%",
            (unsigned long)(session.idle_timeout_ms / 1000),
            (unsigned long)(session.dim_timeout_ms / 1000),
            (unsigned long)(session.sleep_timeout_ms / 1000),
            (unsigned long)(session.deep_sleep_timeout_ms / 60000),
            session.active_brightness,
            session.dim_brightness);
        lv_label_set_text_fmt(
            services_label,
            "%s",
            services_text);
        ui_manager_unlock();
    }
}

static void settings_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)data;

    if (!settings_visible) {
        return;
    }

    if (id == EVENT_SERVICE_STATE ||
        id == EVENT_SESSION_STATE ||
        id == EVENT_APP_STATE ||
        id == EVENT_BATTERY_UPDATED ||
        id == EVENT_TIME_UPDATED) {
        settings_refresh();
    }
}

static void apply_preset(const session_config_t *preset, const char *name)
{
    if (preset == NULL) {
        return;
    }

    if (session_manager_set_config(preset) == ESP_OK) {
        if (ui_manager_lock(300)) {
            lv_label_set_text_fmt(preset_label, "Preset: %s", name);
            ui_manager_unlock();
        }
    } else if (ui_manager_lock(300)) {
        lv_label_set_text(preset_label, "Preset: save failed");
        ui_manager_unlock();
    }

    settings_refresh();
}

static void set_short(lv_event_t *event)
{
    (void)event;
    apply_preset(&preset_short, "Short");
}

static void set_normal(lv_event_t *event)
{
    (void)event;
    apply_preset(&preset_normal, "Normal");
}

static void set_long(lv_event_t *event)
{
    (void)event;
    apply_preset(&preset_long, "Long");
}

static void restart_service_action(const char *name)
{
    if (name == NULL) {
        return;
    }

    esp_err_t err = service_manager_restart(name);
    if (err == ESP_OK) {
        if (ui_manager_lock(300)) {
            lv_label_set_text_fmt(action_label, "%s restart requested", name);
            ui_manager_unlock();
        }
    } else if (ui_manager_lock(300)) {
        lv_label_set_text_fmt(action_label, "%s restart failed", name);
        ui_manager_unlock();
    }
    settings_refresh();
}

static void restart_battery(lv_event_t *event)
{
    (void)event;
    restart_service_action("battery");
}

static void restart_time(lv_event_t *event)
{
    (void)event;
    restart_service_action("time");
}

static esp_err_t settings_init(void)
{
    if (!ui_manager_lock(1000)) return ESP_ERR_TIMEOUT;
    screen = ui_manager_create_screen();
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Settings");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, ui_theme_text_primary(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *overview_panel = lv_obj_create(screen);
    ui_theme_apply_panel(overview_panel);
    lv_obj_set_size(overview_panel, 178, 182);
    lv_obj_align(overview_panel, LV_ALIGN_TOP_LEFT, 16, 54);
    lv_obj_clear_flag(overview_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *overview_title = lv_label_create(overview_panel);
    lv_label_set_text(overview_title, "Overview");
    lv_obj_set_style_text_color(overview_title, ui_theme_text_primary(), 0);
    lv_obj_align(overview_title, LV_ALIGN_TOP_LEFT, 0, 0);

    overview_label = lv_label_create(overview_panel);
    lv_obj_set_width(overview_label, 150);
    lv_label_set_long_mode(overview_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_color(overview_label, ui_theme_text_secondary(), 0);
    lv_obj_align(overview_label, LV_ALIGN_TOP_LEFT, 0, 28);

    lv_obj_t *display_panel = lv_obj_create(screen);
    ui_theme_apply_panel(display_panel);
    lv_obj_set_size(display_panel, 178, 182);
    lv_obj_align(display_panel, LV_ALIGN_TOP_RIGHT, -16, 54);
    lv_obj_clear_flag(display_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *display_title = lv_label_create(display_panel);
    lv_label_set_text(display_title, "Display");
    lv_obj_set_style_text_color(display_title, ui_theme_text_primary(), 0);
    lv_obj_align(display_title, LV_ALIGN_TOP_LEFT, 0, 0);

    display_label = lv_label_create(display_panel);
    lv_obj_set_width(display_label, 150);
    lv_label_set_long_mode(display_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(display_label, "Idle --s\nDim --s\nSleep --s\nDeep --m\nActive --%\nDim --%");
    lv_obj_set_style_text_color(display_label, ui_theme_text_secondary(), 0);
    lv_obj_align(display_label, LV_ALIGN_TOP_LEFT, 0, 28);

    lv_obj_t *services_panel = lv_obj_create(screen);
    ui_theme_apply_panel(services_panel);
    lv_obj_set_size(services_panel, 370, 98);
    lv_obj_align(services_panel, LV_ALIGN_TOP_MID, 0, 246);
    lv_obj_clear_flag(services_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *services_title = lv_label_create(services_panel);
    lv_label_set_text(services_title, "Services");
    lv_obj_set_style_text_color(services_title, ui_theme_text_primary(), 0);
    lv_obj_align(services_title, LV_ALIGN_TOP_LEFT, 0, 0);

    services_label = lv_label_create(services_panel);
    lv_obj_set_width(services_label, 342);
    lv_label_set_long_mode(services_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_color(services_label, ui_theme_text_secondary(), 0);
    lv_obj_align(services_label, LV_ALIGN_TOP_LEFT, 0, 24);

    preset_label = lv_label_create(screen);
    lv_label_set_text(preset_label, "Preset: Normal");
    lv_obj_set_width(preset_label, 220);
    lv_label_set_long_mode(preset_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(preset_label, ui_theme_accent(), 0);
    lv_obj_align(preset_label, LV_ALIGN_BOTTOM_MID, 0, -140);

    action_label = lv_label_create(screen);
    lv_label_set_text(action_label, "Service action: idle");
    lv_obj_set_width(action_label, 260);
    lv_obj_set_style_text_align(action_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(action_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(action_label, ui_theme_text_secondary(), 0);
    lv_obj_align(action_label, LV_ALIGN_BOTTOM_MID, 0, -112);

    lv_obj_t *battery_button = lv_button_create(screen);
    lv_obj_set_size(battery_button, 120, 38);
    lv_obj_align(battery_button, LV_ALIGN_BOTTOM_LEFT, 24, -68);
    lv_obj_add_event_cb(battery_button, restart_battery, LV_EVENT_CLICKED, NULL);
    lv_obj_t *battery_label = lv_label_create(battery_button);
    lv_label_set_text(battery_label, "Restart PMIC");
    lv_obj_set_width(battery_label, 104);
    lv_label_set_long_mode(battery_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_center(battery_label);

    lv_obj_t *time_button = lv_button_create(screen);
    lv_obj_set_size(time_button, 120, 38);
    lv_obj_align(time_button, LV_ALIGN_BOTTOM_RIGHT, -24, -68);
    lv_obj_add_event_cb(time_button, restart_time, LV_EVENT_CLICKED, NULL);
    lv_obj_t *time_label = lv_label_create(time_button);
    lv_label_set_text(time_label, "Restart RTC");
    lv_obj_set_width(time_label, 104);
    lv_label_set_long_mode(time_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_center(time_label);

    lv_obj_t *short_button = lv_button_create(screen);
    lv_obj_set_size(short_button, 90, 40);
    lv_obj_align(short_button, LV_ALIGN_BOTTOM_LEFT, 24, -28);
    lv_obj_add_event_cb(short_button, set_short, LV_EVENT_CLICKED, NULL);
    lv_obj_t *short_label = lv_label_create(short_button);
    lv_label_set_text(short_label, "Short");
    lv_obj_center(short_label);

    lv_obj_t *normal_button = lv_button_create(screen);
    lv_obj_set_size(normal_button, 90, 40);
    lv_obj_align(normal_button, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_add_event_cb(normal_button, set_normal, LV_EVENT_CLICKED, NULL);
    lv_obj_t *normal_label = lv_label_create(normal_button);
    lv_label_set_text(normal_label, "Normal");
    lv_obj_center(normal_label);

    lv_obj_t *long_button = lv_button_create(screen);
    lv_obj_set_size(long_button, 90, 40);
    lv_obj_align(long_button, LV_ALIGN_BOTTOM_RIGHT, -24, -28);
    lv_obj_add_event_cb(long_button, set_long, LV_EVENT_CLICKED, NULL);
    lv_obj_t *long_label = lv_label_create(long_button);
    lv_label_set_text(long_label, "Long");
    lv_obj_center(long_label);

    ui_manager_unlock();
    event_bus_subscribe(EVENT_SERVICE_STATE, settings_event_handler, NULL);
    event_bus_subscribe(EVENT_SESSION_STATE, settings_event_handler, NULL);
    event_bus_subscribe(EVENT_APP_STATE, settings_event_handler, NULL);
    event_bus_subscribe(EVENT_BATTERY_UPDATED, settings_event_handler, NULL);
    event_bus_subscribe(EVENT_TIME_UPDATED, settings_event_handler, NULL);
    return ESP_OK;
}

static esp_err_t settings_enter(void *param)
{
    (void)param;
    settings_visible = true;
    settings_refresh();
    return screen_manager_show(screen);
}

static esp_err_t settings_pause(void)
{
    settings_visible = false;
    return ESP_OK;
}

static esp_err_t settings_resume(void)
{
    settings_visible = true;
    settings_refresh();
    return screen_manager_show(screen);
}

const app_t *settings_app_descriptor(void)
{
    static const app_t app = {
        .id = APP_ID_SETTINGS,
        .name = "Settings",
        .init = settings_init,
        .enter = settings_enter,
        .pause = settings_pause,
        .resume = settings_resume,
    };
    return &app;
}
