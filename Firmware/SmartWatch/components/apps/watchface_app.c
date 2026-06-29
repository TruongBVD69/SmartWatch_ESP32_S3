#include "watchface_app.h"

#include <math.h>
#include <stdlib.h>

#include "battery_service.h"
#include "event_bus.h"
#include "service_manager.h"
#include "session_manager.h"
#include "screen_manager.h"
#include "sensor_service.h"
#include "time_service.h"
#include "ui_events.h"
#include "ui_manager.h"
#include "ui_squareline.h"
#include "ui_theme.h"

static lv_obj_t *screen;
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *battery_label;
static lv_obj_t *service_label;
static lv_obj_t *sensor_label;
static lv_obj_t *session_label;
static lv_obj_t *battery_panel;
static lv_obj_t *motion_panel;
static lv_obj_t *health_panel;

static void refresh_time(const event_time_t *time)
{
    if (ui_squareline_available()) {
        if (!ui_manager_lock(100)) return;
        ui_squareline_watchface_set_time(time->hour, time->minute, time->day, time->month,
                                         (unsigned)time->year);
        ui_manager_unlock();
        return;
    }
    if (time_label == NULL || date_label == NULL) return;
    if (!ui_manager_lock(100)) return;
    lv_label_set_text_fmt(time_label, "%02u:%02u", time->hour, time->minute);
    lv_label_set_text_fmt(date_label, "%02u/%02u/%04u", time->day, time->month,
                          (unsigned)time->year);
    ui_manager_unlock();
}

static void refresh_battery(const event_battery_t *battery)
{
    if (ui_squareline_available()) {
        if (!ui_manager_lock(100)) return;
        ui_squareline_watchface_set_battery(battery->percent, battery->charging);
        ui_manager_unlock();
        return;
    }
    if (battery_label == NULL) return;
    if (!ui_manager_lock(100)) return;
    lv_label_set_text_fmt(battery_label, battery->charging ? "%d%% charging" : "%d%%",
                          battery->percent);
    lv_obj_set_style_text_color(battery_label,
                                battery->charging ? ui_theme_success() : ui_theme_accent(), 0);
    ui_manager_unlock();
}

static void refresh_sensor(const event_sensor_t *sensor)
{
    if (ui_squareline_available()) {
        if (!ui_manager_lock(100)) return;
        ui_squareline_watchface_set_temperature(sensor->temperature_c);
        ui_squareline_watchface_set_steps(sensor->step_count);
        ui_squareline_watchface_set_acceleration(sensor->accel_mag);
        ui_manager_unlock();
        return;
    }
    if (sensor_label == NULL) return;
    if (!ui_manager_lock(100)) return;
    const long temp_tenths = lroundf(sensor->temperature_c * 10.0f);
    const long accel_hundredths = lroundf(sensor->accel_mag * 100.0f);
    lv_label_set_text_fmt(sensor_label, "Temp %ld.%01ldC\nAcc %ld.%02ldg  Step %lu",
                          temp_tenths / 10, labs(temp_tenths % 10),
                          accel_hundredths / 100, labs(accel_hundredths % 100),
                          (unsigned long)sensor->step_count);
    ui_manager_unlock();
}

static void refresh_services(void)
{
    if (service_label == NULL) return;
    const size_t degraded = service_manager_degraded_count();
    const size_t failed = service_manager_failed_count();
    if (!ui_manager_lock(100)) return;
    lv_label_set_text_fmt(service_label, "D:%u  F:%u",
                          (unsigned)degraded, (unsigned)failed);
    lv_obj_set_style_text_color(service_label,
                                failed > 0 ? ui_theme_danger() :
                                degraded > 0 ? ui_theme_warning() :
                                ui_theme_success(), 0);
    ui_manager_unlock();
}

static void refresh_session(const event_session_t *session)
{
    if (session_label == NULL) return;
    static const char *state_names[] = {"ACTIVE", "IDLE", "DIM", "SLEEP"};
    static const char *reason_names[] = {"BOOT", "INPUT", "TIMEOUT", "PWR", "BTN"};
    const char *state = session->state < 4 ? state_names[session->state] : "?";
    const char *reason = session->reason < 5 ? reason_names[session->reason] : "?";
    if (!ui_manager_lock(100)) return;
    lv_label_set_text_fmt(session_label, "%s  %s  %u%%", state, reason, session->brightness);
    ui_manager_unlock();
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == EVENT_TIME_UPDATED) refresh_time(data);
    if (id == EVENT_BATTERY_UPDATED) refresh_battery(data);
    if (id == EVENT_SENSOR_UPDATED) refresh_sensor(data);
    if (id == EVENT_SERVICE_STATE) refresh_services();
    if (id == EVENT_SESSION_STATE) refresh_session(data);
}

static void launcher_clicked(lv_event_t *event)
{
    (void)event;
    ui_events_open_app(APP_ID_LAUNCHER);
}

static esp_err_t watchface_init(void)
{
    if (!ui_manager_lock(1000)) return ESP_ERR_TIMEOUT;
    if (ui_squareline_available()) {
        screen = ui_squareline_create_screen(UI_SQUARELINE_SCREEN_WATCHFACE);
        if (screen != NULL) {
            (void)ui_squareline_watchface_bind(&time_label, &date_label, &battery_label);
            sensor_label = NULL;
            service_label = NULL;
            session_label = NULL;
            battery_panel = NULL;
            motion_panel = NULL;
            health_panel = NULL;
            ui_manager_unlock();

            event_bus_subscribe(EVENT_TIME_UPDATED, event_handler, NULL);
            event_bus_subscribe(EVENT_BATTERY_UPDATED, event_handler, NULL);
            event_bus_subscribe(EVENT_SENSOR_UPDATED, event_handler, NULL);
            event_bus_subscribe(EVENT_SERVICE_STATE, event_handler, NULL);
            event_bus_subscribe(EVENT_SESSION_STATE, event_handler, NULL);
            return ESP_OK;
        }
    }

    screen = ui_manager_create_screen();

    lv_obj_t *brand = lv_label_create(screen);
    lv_label_set_text(brand, "SmartWatch OS");
    lv_obj_set_style_text_color(brand, ui_theme_text_primary(), 0);
    lv_obj_set_style_text_font(brand, LV_FONT_DEFAULT, 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 20);

    battery_panel = lv_obj_create(screen);
    ui_theme_apply_panel(battery_panel);
    lv_obj_set_size(battery_panel, 162, 54);
    lv_obj_align(battery_panel, LV_ALIGN_TOP_LEFT, 20, 48);
    lv_obj_clear_flag(battery_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *battery_title = lv_label_create(battery_panel);
    lv_label_set_text(battery_title, "Battery");
    lv_obj_set_style_text_color(battery_title, ui_theme_text_secondary(), 0);
    lv_obj_align(battery_title, LV_ALIGN_TOP_LEFT, 0, 0);

    battery_label = lv_label_create(battery_panel);
    lv_label_set_text(battery_label, "--%");
    lv_obj_set_width(battery_label, 132);
    lv_label_set_long_mode(battery_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(battery_label, ui_theme_accent(), 0);
    lv_obj_align(battery_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *session_panel = lv_obj_create(screen);
    ui_theme_apply_panel(session_panel);
    lv_obj_set_size(session_panel, 162, 54);
    lv_obj_align(session_panel, LV_ALIGN_TOP_RIGHT, -20, 48);
    lv_obj_clear_flag(session_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *session_title = lv_label_create(session_panel);
    lv_label_set_text(session_title, "Session");
    lv_obj_set_style_text_color(session_title, ui_theme_text_secondary(), 0);
    lv_obj_align(session_title, LV_ALIGN_TOP_LEFT, 0, 0);

    session_label = lv_label_create(session_panel);
    lv_label_set_text(session_label, "ACTIVE  BOOT  100%");
    lv_obj_set_width(session_label, 132);
    lv_label_set_long_mode(session_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_color(session_label, ui_theme_text_primary(), 0);
    lv_obj_align(session_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "--:--");
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
#endif
    lv_obj_set_style_text_color(time_label, ui_theme_text_primary(), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -26);

    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "--/--/----");
    lv_obj_set_style_text_color(date_label, ui_theme_text_secondary(), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 18);

    motion_panel = lv_obj_create(screen);
    ui_theme_apply_panel(motion_panel);
    lv_obj_set_size(motion_panel, 162, 84);
    lv_obj_align(motion_panel, LV_ALIGN_BOTTOM_LEFT, 20, -92);
    lv_obj_clear_flag(motion_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *motion_title = lv_label_create(motion_panel);
    lv_label_set_text(motion_title, "Motion");
    lv_obj_set_style_text_color(motion_title, ui_theme_text_secondary(), 0);
    lv_obj_align(motion_title, LV_ALIGN_TOP_LEFT, 0, 0);

    sensor_label = lv_label_create(motion_panel);
    lv_obj_set_width(sensor_label, 132);
    lv_label_set_long_mode(sensor_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(sensor_label, "Temp --.-C\nGyro --.-  Acc --.--");
    lv_obj_set_style_text_color(sensor_label, ui_theme_text_primary(), 0);
    lv_obj_align(sensor_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    health_panel = lv_obj_create(screen);
    ui_theme_apply_panel(health_panel);
    lv_obj_set_size(health_panel, 162, 84);
    lv_obj_align(health_panel, LV_ALIGN_BOTTOM_RIGHT, -20, -92);
    lv_obj_clear_flag(health_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *health_title = lv_label_create(health_panel);
    lv_label_set_text(health_title, "Platform");
    lv_obj_set_style_text_color(health_title, ui_theme_text_secondary(), 0);
    lv_obj_align(health_title, LV_ALIGN_TOP_LEFT, 0, 0);

    service_label = lv_label_create(health_panel);
    lv_obj_set_width(service_label, 132);
    lv_label_set_long_mode(service_label, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_text(service_label, "D:0  F:0");
    lv_obj_set_style_text_color(service_label, ui_theme_success(), 0);
    lv_obj_align(service_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *launcher = lv_button_create(screen);
    lv_obj_set_size(launcher, 132, 44);
    lv_obj_align(launcher, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_add_event_cb(launcher, launcher_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(launcher);
    lv_label_set_text(label, "Launcher");
    lv_obj_center(label);
    ui_manager_unlock();

    event_bus_subscribe(EVENT_TIME_UPDATED, event_handler, NULL);
    event_bus_subscribe(EVENT_BATTERY_UPDATED, event_handler, NULL);
    event_bus_subscribe(EVENT_SENSOR_UPDATED, event_handler, NULL);
    event_bus_subscribe(EVENT_SERVICE_STATE, event_handler, NULL);
    event_bus_subscribe(EVENT_SESSION_STATE, event_handler, NULL);
    return ESP_OK;
}

static esp_err_t watchface_enter(void *param)
{
    (void)param;
    platform_datetime_t time;
    if (time_service_get(&time) == ESP_OK) {
        const event_time_t event = {
            .year = time.year, .month = time.month, .day = time.day,
            .hour = time.hour, .minute = time.minute, .second = time.second,
        };
        refresh_time(&event);
    }
    platform_power_status_t power;
    if (battery_service_get(&power) == ESP_OK) {
        const event_battery_t event = {
            .present = power.battery_present, .charging = power.charging,
            .percent = power.battery_percent, .millivolts = power.battery_mv,
        };
        refresh_battery(&event);
    }
    platform_imu_sample_t imu;
    if (sensor_service_read(&imu) == ESP_OK) {
        sensor_service_step_state_t step_state = {0};
        (void)sensor_service_get_step_state(&step_state);
        const event_sensor_t event = {
            .temperature_c = imu.temperature_c,
            .accel_mag = sqrtf(imu.accel_x * imu.accel_x +
                               imu.accel_y * imu.accel_y +
                               imu.accel_z * imu.accel_z),
            .gyro_mag = sqrtf(imu.gyro_x * imu.gyro_x +
                              imu.gyro_y * imu.gyro_y +
                              imu.gyro_z * imu.gyro_z),
            .step_count = step_state.step_count,
            .cadence_spm = step_state.cadence_spm,
            .step_confidence = step_state.step_confidence,
            .walking = step_state.walking,
        };
        refresh_sensor(&event);
    }
    const session_status_t session = session_manager_get_status();
    const event_session_t session_event = {
        .state = session.state,
        .reason = session.reason,
        .brightness = session.brightness,
        .idle_ms = session.idle_ms,
    };
    refresh_session(&session_event);
    refresh_services();
    return screen_manager_show(screen);
}

const app_t *watchface_app_descriptor(void)
{
    static const app_t app = {
        .id = APP_ID_WATCHFACE, .name = "WatchFace", .init = watchface_init,
        .enter = watchface_enter,
    };
    return &app;
}
