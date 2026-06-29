#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_codec_dev.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "lvgl.h"
#include "pcf85063a.h"
#include "qmi8658.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool init_display;
    bool init_imu;
    bool init_rtc;
    bool init_power;
    bool init_buttons;
} smartwatch_board_config_t;

#define SMARTWATCH_BOARD_CONFIG_DEFAULT() \
    {                                      \
        .init_display = true,              \
        .init_imu = true,                  \
        .init_rtc = true,                  \
        .init_power = true,                \
        .init_buttons = true,              \
    }

typedef struct {
    bool display_ready;
    bool touch_ready;
    bool imu_ready;
    bool rtc_ready;
    bool power_ready;
    bool buttons_ready;
} smartwatch_board_status_t;

typedef struct {
    bool battery_present;
    bool vbus_present;
    bool charging;
    bool discharging;
    int battery_percent;
    uint16_t battery_mv;
    uint16_t vbus_mv;
    uint16_t system_mv;
    float pmic_temperature_c;
} smartwatch_board_power_status_t;

typedef enum {
    SMARTWATCH_POWER_EVENT_BATTERY_INSERTED,
    SMARTWATCH_POWER_EVENT_BATTERY_REMOVED,
    SMARTWATCH_POWER_EVENT_VBUS_INSERTED,
    SMARTWATCH_POWER_EVENT_VBUS_REMOVED,
    SMARTWATCH_POWER_EVENT_CHARGE_STARTED,
    SMARTWATCH_POWER_EVENT_CHARGE_STOPPED,
    SMARTWATCH_POWER_EVENT_CHARGE_COMPLETED,
} smartwatch_board_power_event_type_t;

typedef struct {
    smartwatch_board_power_event_type_t type;
    smartwatch_board_power_status_t status;
} smartwatch_board_power_event_t;

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
} smartwatch_board_audio_config_t;

#define SMARTWATCH_BOARD_AUDIO_CONFIG_DEFAULT() \
    {                                            \
        .sample_rate_hz = 16000,                 \
        .channels = 1,                           \
        .bits_per_sample = 16,                   \
    }

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} smartwatch_board_motion_calibration_t;

typedef enum {
    SMARTWATCH_BUTTON_BOOT,
    SMARTWATCH_BUTTON_POWER,
} smartwatch_board_button_t;

typedef enum {
    SMARTWATCH_BUTTON_EVENT_PRESS,
    SMARTWATCH_BUTTON_EVENT_RELEASE,
    SMARTWATCH_BUTTON_EVENT_CLICK,
    SMARTWATCH_BUTTON_EVENT_LONG_PRESS,
} smartwatch_board_button_event_type_t;

typedef struct {
    smartwatch_board_button_t button;
    smartwatch_board_button_event_type_t type;
    uint32_t duration_ms;
} smartwatch_board_button_event_t;

esp_err_t smartwatch_board_init(const smartwatch_board_config_t *config);
const smartwatch_board_status_t *smartwatch_board_get_status(void);

lv_display_t *smartwatch_board_get_display(void);
lv_indev_t *smartwatch_board_get_touch_input(void);
bool smartwatch_board_display_lock(uint32_t timeout_ms);
void smartwatch_board_display_unlock(void);
esp_err_t smartwatch_board_display_set_brightness(uint8_t percent);
uint8_t smartwatch_board_display_get_brightness(void);
esp_err_t smartwatch_board_display_set_backlight(bool enabled);

qmi8658_dev_t *smartwatch_board_get_imu(void);
pcf85063a_dev_t *smartwatch_board_get_rtc(void);
i2c_master_bus_handle_t smartwatch_board_get_i2c_bus(void);

esp_err_t smartwatch_board_sdcard_mount(void);
esp_err_t smartwatch_board_sdcard_unmount(void);
esp_codec_dev_handle_t smartwatch_board_speaker_init(void);
esp_codec_dev_handle_t smartwatch_board_microphone_init(void);
esp_err_t smartwatch_board_audio_deinit(void);
esp_err_t smartwatch_board_audio_start(const smartwatch_board_audio_config_t *config,
                                       bool enable_speaker, bool enable_microphone);
esp_err_t smartwatch_board_audio_stop(void);
esp_err_t smartwatch_board_audio_write(const void *data, size_t size);
esp_err_t smartwatch_board_audio_read(void *data, size_t size);
esp_err_t smartwatch_board_audio_set_volume(float volume_percent);
esp_err_t smartwatch_board_audio_set_input_gain(float gain_db);

esp_err_t smartwatch_board_power_get_status(smartwatch_board_power_status_t *status);
esp_err_t smartwatch_board_power_get_event(smartwatch_board_power_event_t *event, uint32_t timeout_ms);
esp_err_t smartwatch_board_power_off(void);
uint64_t smartwatch_board_get_wakeup_ext1_mask(void);
gpio_num_t smartwatch_board_get_wakeup_ext0_gpio(void);
int smartwatch_board_get_wakeup_ext0_level(void);
bool smartwatch_board_touch_wakeup_pending(void);

esp_err_t smartwatch_board_motion_calibrate(smartwatch_board_motion_calibration_t *calibration,
                                            size_t sample_count, uint32_t sample_interval_ms);
esp_err_t smartwatch_board_motion_read(const smartwatch_board_motion_calibration_t *calibration,
                                       qmi8658_data_t *data);

esp_err_t smartwatch_board_button_get_event(smartwatch_board_button_event_t *event, uint32_t timeout_ms);
bool smartwatch_board_button_is_pressed(smartwatch_board_button_t button);

#ifdef __cplusplus
}
#endif
