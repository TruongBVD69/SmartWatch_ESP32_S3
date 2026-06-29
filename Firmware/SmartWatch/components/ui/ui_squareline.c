#include "ui_squareline.h"

#include <stdint.h>
#include <stdio.h>

#include "ui.h"
#include "ui_events.h"

static bool initialized;

static void squareline_open_app_cb(lv_event_t *event)
{
    const app_id_t app_id = (app_id_t)(intptr_t)lv_event_get_user_data(event);
    ui_events_open_app(app_id);
}

static void squareline_back_cb(lv_event_t *event)
{
    (void)event;
    ui_events_back();
}

esp_err_t ui_squareline_init(void)
{
    if (initialized) {
        return ESP_OK;
    }
    if (LV_EVENT_GET_COMP_CHILD == 0) {
        LV_EVENT_GET_COMP_CHILD = lv_event_register_id();
    }
    ui_watch_digital_screen_init();
    ui_weather_1_screen_init();
    initialized = true;
    return ESP_OK;
}

bool ui_squareline_available(void)
{
    return initialized;
}

lv_obj_t *ui_squareline_create_screen(ui_squareline_screen_id_t screen_id)
{
    if (!initialized) {
        return NULL;
    }

    switch (screen_id) {
    case UI_SQUARELINE_SCREEN_WATCHFACE:
    default:
        return ui_watch_digital;
    case UI_SQUARELINE_SCREEN_LAUNCHER:
    case UI_SQUARELINE_SCREEN_SETTINGS:
    case UI_SQUARELINE_SCREEN_DIAGNOSTICS:
        return NULL;
    }
}

bool ui_squareline_watchface_bind(lv_obj_t **time_label, lv_obj_t **date_label,
                                  lv_obj_t **battery_label)
{
    if (!initialized || ui_watch_digital == NULL) {
        return false;
    }
    if (time_label != NULL) {
        *time_label = NULL;
    }
    if (date_label != NULL) {
        *date_label = NULL;
    }
    if (battery_label != NULL) {
        *battery_label = NULL;
    }
    return true;
}

static void set_title_subtitle(lv_obj_t *group, const char *title, const char *subtitle)
{
    if (group == NULL) {
        return;
    }

    lv_obj_t *title_label = ui_comp_get_child(group, UI_COMP_TITLEGROUP_TITLE);
    lv_obj_t *subtitle_label = ui_comp_get_child(group, UI_COMP_TITLEGROUP_SUBTITLE);

    if (title != NULL && title_label != NULL) {
        lv_label_set_text(title_label, title);
    }
    if (subtitle != NULL && subtitle_label != NULL) {
        lv_label_set_text(subtitle_label, subtitle);
    }
}

void ui_squareline_watchface_set_time(unsigned hour, unsigned minute, unsigned day,
                                      unsigned month, unsigned year)
{
    static const char *month_names[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };

    if (!initialized) {
        return;
    }

    if (ui_label_hour_1 != NULL) {
        lv_label_set_text_fmt(ui_label_hour_1, "%u", (hour / 10U) % 10U);
    }
    if (ui_label_hour_2 != NULL) {
        lv_label_set_text_fmt(ui_label_hour_2, "%u", hour % 10U);
    }
    if (ui_label_min != NULL) {
        lv_label_set_text_fmt(ui_label_min, "%02u", minute % 100U);
    }

    if (ui_date_group != NULL) {
        lv_obj_t *day_label = ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_DAY);
        lv_obj_t *month_label = ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_MONTH);
        lv_obj_t *year_label = ui_comp_get_child(ui_date_group, UI_COMP_DATEGROUP_YEAR);
        if (day_label != NULL) {
            lv_label_set_text(day_label, "TODAY");
        }
        if (month_label != NULL) {
            const char *month_name = (month >= 1U && month <= 12U) ? month_names[month - 1U] : "--";
            lv_label_set_text_fmt(month_label, "%02u %s", day % 100U, month_name);
        }
        if (year_label != NULL) {
            lv_label_set_text_fmt(year_label, "%04u", year % 10000U);
        }
    }

    if (ui_city_gruop_1 != NULL) {
        char subtitle[32];
        snprintf(subtitle, sizeof(subtitle), "%02u.%02u.%04u", day % 100U, month % 100U,
                 year % 10000U);
        set_title_subtitle(ui_city_gruop_1, NULL, subtitle);
    }
}

void ui_squareline_watchface_set_battery(int percent, bool charging)
{
    if (!initialized || ui_battery_group == NULL) {
        return;
    }

    lv_obj_t *battery_label = ui_comp_get_child(ui_battery_group,
                                                UI_COMP_BATTERYGROUP_BATTERY_PERCENT);
    if (battery_label != NULL) {
        lv_label_set_text_fmt(battery_label, charging ? "%d%%+" : "%d%%", percent);
    }
}

void ui_squareline_watchface_set_temperature(float temperature_c)
{
    if (!initialized) {
        return;
    }

    const int rounded_temp = (int)((temperature_c >= 0.0f) ? temperature_c + 0.5f
                                                           : temperature_c - 0.5f);
    if (ui_weather_group_1 != NULL) {
        lv_obj_t *degree_label = ui_comp_get_child(ui_weather_group_1,
                                                   UI_COMP_WEATHERGROUP1_DEGREE_1);
        if (degree_label != NULL) {
            lv_label_set_text_fmt(degree_label, "%d\xC2\xB0", rounded_temp);
        }
    }
    if (ui_label_degree != NULL) {
        lv_label_set_text_fmt(ui_label_degree, "%d\xC2\xB0", rounded_temp);
    }
}

void ui_squareline_watchface_set_steps(uint32_t step_count)
{
    if (!initialized || ui_step_group == NULL) {
        return;
    }

    lv_obj_t *step_label = ui_comp_get_child(ui_step_group, UI_COMP_STEPGROUP_STEP_LABEL);
    if (step_label != NULL) {
        lv_label_set_text_fmt(step_label, "%lu", (unsigned long)step_count);
    }
}

void ui_squareline_watchface_set_acceleration(float accel_mag)
{
    if (!initialized) {
        return;
    }

    if (ui_weather_title_group_1 != NULL) {
        set_title_subtitle(ui_weather_title_group_1, NULL, NULL);
        lv_obj_t *subtitle = ui_comp_get_child(ui_weather_title_group_1, UI_COMP_TITLEGROUP_SUBTITLE);
        if (subtitle != NULL) {
            lv_label_set_text_fmt(subtitle, "Acc %.02fg", accel_mag);
        }
    }

    if (ui_weather_title_group_3 != NULL) {
        lv_obj_t *subtitle = ui_comp_get_child(ui_weather_title_group_3, UI_COMP_TITLEGROUP_SUBTITLE);
        if (subtitle != NULL) {
            lv_label_set_text_fmt(subtitle, "Acc %.02fg", accel_mag);
        }
    }
}

void ui_squareline_launcher_setup(void)
{
    if (!initialized) {
        return;
    }
}

void ui_squareline_bind_open_app(lv_obj_t *obj, app_id_t app_id)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_add_event_cb(obj, squareline_open_app_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)app_id);
}

void ui_squareline_bind_back(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_add_event_cb(obj, squareline_back_cb, LV_EVENT_CLICKED, NULL);
}
