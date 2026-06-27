#include "smartwatch_ui.h"

#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "lvgl.h"
#include "smartwatch_board.h"
#include "smartwatch_platform.h"

static const char *TAG = "ui";
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_obj_t *battery_label;
static lv_obj_t *system_label;

static void set_system_text(const char *text, lv_color_t color)
{
    if (!smartwatch_board_display_lock(100)) return;
    lv_label_set_text(system_label, text);
    lv_obj_set_style_text_color(system_label, color, 0);
    smartwatch_board_display_unlock();
}

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id == SMARTWATCH_EVENT_RTC_TICK) {
        const smartwatch_rtc_event_t *event = data;
        if (!smartwatch_board_display_lock(100)) return;
        lv_label_set_text_fmt(time_label, "%02u:%02u", event->hour, event->minute);
        lv_label_set_text_fmt(date_label, "%02u/%02u/%04u", event->day, event->month,
                              (unsigned)event->year);
        smartwatch_board_display_unlock();
    } else if (id == SMARTWATCH_EVENT_POWER_STATUS || id == SMARTWATCH_EVENT_POWER_CHANGED) {
        const smartwatch_power_event_t *event = data;
        if (!smartwatch_board_display_lock(100)) return;
        lv_label_set_text_fmt(battery_label, event->charging ? "BAT %d%%  CHARGING" : "BAT %d%%",
                              event->battery_percent);
        lv_obj_set_style_text_color(battery_label,
                                    event->battery_percent <= 15 ? lv_palette_main(LV_PALETTE_RED)
                                                                 : lv_palette_main(LV_PALETTE_GREEN), 0);
        smartwatch_board_display_unlock();
    } else if (id == SMARTWATCH_EVENT_SERVICE_STATE) {
        const smartwatch_service_event_t *event = data;
        if (event->state == SMARTWATCH_SERVICE_DEGRADED || event->state == SMARTWATCH_SERVICE_FAILED) {
            char message[48];
            snprintf(message, sizeof(message), "%s %s",
                     smartwatch_platform_service_name(event->service),
                     smartwatch_platform_service_state_name(event->state));
            set_system_text(message, lv_palette_main(LV_PALETTE_AMBER));
        }
    } else if (id == SMARTWATCH_EVENT_BUTTON) {
        const smartwatch_input_event_t *event = data;
        if (event->action == SMARTWATCH_INPUT_CLICK) {
            set_system_text(event->input == SMARTWATCH_INPUT_POWER ? "Power button" : "Boot button",
                            lv_palette_lighten(LV_PALETTE_BLUE, 2));
        }
    } else if (id == SMARTWATCH_EVENT_BOOT_COMPLETED) {
        set_system_text("System ready", lv_palette_main(LV_PALETTE_GREEN));
    }
}

static void create_home_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x050708), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *brand = lv_label_create(screen);
    lv_label_set_text(brand, "SMARTWATCH OS");
    lv_obj_set_style_text_color(brand, lv_color_hex(0xE8ECEF), 0);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 24);

    battery_label = lv_label_create(screen);
    lv_label_set_text(battery_label, "BAT --%");
    lv_obj_set_style_text_color(battery_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_MID, 0, 62);

    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "--:--");
#if LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_32, 0);
#endif
    lv_obj_set_style_text_color(time_label, lv_color_white(), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -34);

    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "--/--/----");
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xAAB3B9), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 12);

    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 270, 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x263137), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_align(divider, LV_ALIGN_BOTTOM_MID, 0, -82);

    system_label = lv_label_create(screen);
    lv_label_set_text(system_label, "Starting services");
    lv_obj_set_width(system_label, 320);
    lv_obj_set_style_text_align(system_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(system_label, lv_palette_main(LV_PALETTE_AMBER), 0);
    lv_obj_align(system_label, LV_ALIGN_BOTTOM_MID, 0, -42);
}

esp_err_t smartwatch_ui_start(void)
{
    if (!smartwatch_board_get_status()->display_ready) return ESP_ERR_INVALID_STATE;
    if (!smartwatch_board_display_lock(1000)) return ESP_ERR_TIMEOUT;
    create_home_screen();
    smartwatch_board_display_unlock();

    const int32_t ids[] = {
        SMARTWATCH_EVENT_RTC_TICK, SMARTWATCH_EVENT_POWER_STATUS,
        SMARTWATCH_EVENT_POWER_CHANGED, SMARTWATCH_EVENT_SERVICE_STATE,
        SMARTWATCH_EVENT_BUTTON, SMARTWATCH_EVENT_BOOT_COMPLETED,
    };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        esp_err_t err = esp_event_handler_register(SMARTWATCH_EVENTS, ids[i], event_handler, NULL);
        if (err != ESP_OK) return err;
    }
    ESP_LOGI(TAG, "Home screen ready");
    return smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_UI,
                                                     SMARTWATCH_SERVICE_READY, ESP_OK);
}
