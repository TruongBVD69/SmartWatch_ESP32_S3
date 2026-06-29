#include "smartwatch_board.h"

#include <stdlib.h>
#include <string.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "smartwatch_board";

static bool initialized;
static lv_display_t *display;
static qmi8658_dev_t imu;
static pcf85063a_dev_t rtc;
static smartwatch_board_status_t status;
static QueueHandle_t button_queue;
static QueueHandle_t power_event_queue;

#define SMARTWATCH_DISPLAY_TARGET_BUFFER_LINES 80
#define SMARTWATCH_DISPLAY_DOUBLE_BUFFER       1
#define SMARTWATCH_DISPLAY_BOUNCE_LINES        40

#define SMARTWATCH_TOUCH_WAKE_GPIO BSP_LCD_TOUCH_INT

#define BUTTON_POLL_MS 10
#define BUTTON_DEBOUNCE_MS 30
#define BUTTON_LONG_PRESS_MS 800
#define POWER_POLL_MS 500

typedef struct {
    smartwatch_board_button_t id;
    gpio_num_t gpio;
    bool active_high;
    bool raw_pressed;
    bool stable_pressed;
    bool long_sent;
    TickType_t raw_changed_at;
    TickType_t pressed_at;
} button_state_t;

static button_state_t buttons[] = {
    {.id = SMARTWATCH_BUTTON_BOOT, .gpio = GPIO_NUM_0, .active_high = false},
    {.id = SMARTWATCH_BUTTON_POWER, .gpio = GPIO_NUM_10, .active_high = true},
};

extern esp_err_t smartwatch_power_driver_init(i2c_master_bus_handle_t bus);
extern esp_err_t smartwatch_power_driver_get_status(smartwatch_board_power_status_t *power_status);
extern esp_err_t smartwatch_power_driver_shutdown(void);

static void power_send_event(smartwatch_board_power_event_type_t type,
                             const smartwatch_board_power_status_t *power_status)
{
    const smartwatch_board_power_event_t event = {
        .type = type,
        .status = *power_status,
    };
    xQueueSend(power_event_queue, &event, 0);
}

static void power_event_task(void *arg)
{
    smartwatch_board_power_status_t previous = *(smartwatch_board_power_status_t *)arg;
    free(arg);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(POWER_POLL_MS));
        smartwatch_board_power_status_t current = {0};
        if (smartwatch_power_driver_get_status(&current) != ESP_OK) {
            continue;
        }
        if (current.battery_present != previous.battery_present) {
            power_send_event(current.battery_present ? SMARTWATCH_POWER_EVENT_BATTERY_INSERTED
                                                     : SMARTWATCH_POWER_EVENT_BATTERY_REMOVED,
                             &current);
        }
        if (current.vbus_present != previous.vbus_present) {
            power_send_event(current.vbus_present ? SMARTWATCH_POWER_EVENT_VBUS_INSERTED
                                                  : SMARTWATCH_POWER_EVENT_VBUS_REMOVED,
                             &current);
        }
        if (current.charging != previous.charging) {
            smartwatch_board_power_event_type_t type = SMARTWATCH_POWER_EVENT_CHARGE_STARTED;
            if (!current.charging) {
                type = current.vbus_present && current.battery_percent >= 99
                           ? SMARTWATCH_POWER_EVENT_CHARGE_COMPLETED
                           : SMARTWATCH_POWER_EVENT_CHARGE_STOPPED;
            }
            power_send_event(type, &current);
        }
        previous = current;
    }
}

static esp_err_t init_power_events(void)
{
    power_event_queue = xQueueCreate(8, sizeof(smartwatch_board_power_event_t));
    if (power_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    smartwatch_board_power_status_t *initial = malloc(sizeof(*initial));
    if (initial == NULL) {
        vQueueDelete(power_event_queue);
        power_event_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = smartwatch_power_driver_get_status(initial);
    if (err != ESP_OK || xTaskCreate(power_event_task, "board_power", 4096, initial, 4, NULL) != pdPASS) {
        free(initial);
        vQueueDelete(power_event_queue);
        power_event_queue = NULL;
        return err == ESP_OK ? ESP_ERR_NO_MEM : err;
    }
    return ESP_OK;
}

static bool button_read(const button_state_t *button)
{
    return gpio_get_level(button->gpio) == (button->active_high ? 1 : 0);
}

static void button_send_event(const button_state_t *button, smartwatch_board_button_event_type_t type,
                              uint32_t duration_ms)
{
    const smartwatch_board_button_event_t event = {
        .button = button->id,
        .type = type,
        .duration_ms = duration_ms,
    };
    xQueueSend(button_queue, &event, 0);
}

static void button_task(void *arg)
{
    (void)arg;
    const TickType_t debounce_ticks = pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS);
    const TickType_t long_ticks = pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS);

    while (true) {
        const TickType_t now = xTaskGetTickCount();
        for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
            button_state_t *button = &buttons[i];
            const bool pressed = button_read(button);
            if (pressed != button->raw_pressed) {
                button->raw_pressed = pressed;
                button->raw_changed_at = now;
            }
            if (pressed != button->stable_pressed && now - button->raw_changed_at >= debounce_ticks) {
                button->stable_pressed = pressed;
                if (pressed) {
                    button->pressed_at = now;
                    button->long_sent = false;
                    button_send_event(button, SMARTWATCH_BUTTON_EVENT_PRESS, 0);
                } else {
                    const uint32_t duration = pdTICKS_TO_MS(now - button->pressed_at);
                    button_send_event(button, SMARTWATCH_BUTTON_EVENT_RELEASE, duration);
                    if (!button->long_sent) {
                        button_send_event(button, SMARTWATCH_BUTTON_EVENT_CLICK, duration);
                    }
                }
            }
            if (button->stable_pressed && !button->long_sent && now - button->pressed_at >= long_ticks) {
                button->long_sent = true;
                button_send_event(button, SMARTWATCH_BUTTON_EVENT_LONG_PRESS,
                                  pdTICKS_TO_MS(now - button->pressed_at));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

static esp_err_t init_buttons(void)
{
    gpio_config_t boot_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&boot_config), TAG, "BOOT button GPIO configuration failed");

    gpio_config_t power_config = {
        .pin_bit_mask = BIT64(GPIO_NUM_10),
        .mode = GPIO_MODE_INPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&power_config), TAG, "PWR button GPIO configuration failed");

    button_queue = xQueueCreate(12, sizeof(smartwatch_board_button_event_t));
    if (button_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const TickType_t now = xTaskGetTickCount();
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        buttons[i].raw_pressed = button_read(&buttons[i]);
        buttons[i].stable_pressed = buttons[i].raw_pressed;
        buttons[i].raw_changed_at = now;
        buttons[i].pressed_at = now;
    }
    return xTaskCreate(button_task, "board_buttons", 3072, NULL, 5, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

static void init_optional_i2c_devices(const smartwatch_board_config_t *config)
{
    if (!config->init_imu && !config->init_rtc) {
        return;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "Shared I2C bus initialization failed");
        return;
    }

    if (config->init_imu) {
        esp_err_t err = qmi8658_init(&imu, bus, QMI8658_ADDRESS_HIGH);
        status.imu_ready = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 unavailable: %s", esp_err_to_name(err));
        }
    }

    if (config->init_rtc) {
        esp_err_t err = pcf85063a_init(&rtc, bus, PCF85063A_ADDRESS);
        status.rtc_ready = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "PCF85063A unavailable: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t smartwatch_board_init(const smartwatch_board_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&status, 0, sizeof(status));

    if (config->init_display) {
        lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
        lvgl_cfg.task_priority = 5;

        const bsp_display_cfg_t display_cfg = {
            .lvgl_port_cfg = lvgl_cfg,
            .buffer_size = BSP_LCD_H_RES * SMARTWATCH_DISPLAY_TARGET_BUFFER_LINES,
            .double_buffer = SMARTWATCH_DISPLAY_DOUBLE_BUFFER,
            .trans_size = BSP_LCD_H_RES * SMARTWATCH_DISPLAY_BOUNCE_LINES,
            .flags = {
                .buff_dma = true,
                .buff_spiram = false,
            },
        };

        display = bsp_display_start_with_config(&display_cfg);
        if (display == NULL) {
            return ESP_FAIL;
        }
        status.display_ready = true;
        status.touch_ready = (bsp_display_get_input_dev() != NULL);
    }

    init_optional_i2c_devices(config);
    if (config->init_power) {
        esp_err_t err = smartwatch_power_driver_init(bsp_i2c_get_handle());
        if (err == ESP_OK) {
            err = init_power_events();
        }
        status.power_ready = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "AXP2101 unavailable: %s", esp_err_to_name(err));
        }
    }
    if (config->init_buttons) {
        esp_err_t err = init_buttons();
        status.buttons_ready = (err == ESP_OK);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Buttons unavailable: %s", esp_err_to_name(err));
        }
    }
    initialized = true;

    ESP_LOGI(TAG, "Ready: display=%d touch=%d imu=%d rtc=%d power=%d buttons=%d",
             status.display_ready, status.touch_ready, status.imu_ready, status.rtc_ready,
             status.power_ready, status.buttons_ready);
    return ESP_OK;
}

const smartwatch_board_status_t *smartwatch_board_get_status(void)
{
    return &status;
}

lv_display_t *smartwatch_board_get_display(void)
{
    return display;
}

lv_indev_t *smartwatch_board_get_touch_input(void)
{
    return status.touch_ready ? bsp_display_get_input_dev() : NULL;
}

bool smartwatch_board_display_lock(uint32_t timeout_ms)
{
    return status.display_ready && bsp_display_lock(timeout_ms);
}

void smartwatch_board_display_unlock(void)
{
    if (status.display_ready) {
        bsp_display_unlock();
    }
}

esp_err_t smartwatch_board_display_set_brightness(uint8_t percent)
{
    if (!status.display_ready || percent > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    return bsp_display_brightness_set(percent);
}

uint8_t smartwatch_board_display_get_brightness(void)
{
    if (!status.display_ready) {
        return 0;
    }
    int brightness = bsp_display_brightness_get();
    if (brightness < 0) {
        return 0;
    }
    return (uint8_t)brightness;
}

esp_err_t smartwatch_board_display_set_backlight(bool enabled)
{
    if (!status.display_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return enabled ? bsp_display_backlight_on() : bsp_display_backlight_off();
}

qmi8658_dev_t *smartwatch_board_get_imu(void)
{
    return status.imu_ready ? &imu : NULL;
}

pcf85063a_dev_t *smartwatch_board_get_rtc(void)
{
    return status.rtc_ready ? &rtc : NULL;
}

i2c_master_bus_handle_t smartwatch_board_get_i2c_bus(void)
{
    return bsp_i2c_get_handle();
}

esp_err_t smartwatch_board_sdcard_mount(void)
{
    return bsp_sdcard_mount();
}

esp_err_t smartwatch_board_sdcard_unmount(void)
{
    return bsp_sdcard_unmount();
}

esp_codec_dev_handle_t smartwatch_board_speaker_init(void)
{
    return bsp_audio_codec_speaker_init();
}

esp_codec_dev_handle_t smartwatch_board_microphone_init(void)
{
    return bsp_audio_codec_microphone_init();
}

esp_err_t smartwatch_board_audio_deinit(void)
{
    return smartwatch_board_audio_stop();
}

esp_err_t smartwatch_board_power_get_status(smartwatch_board_power_status_t *power_status)
{
    if (!status.power_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return smartwatch_power_driver_get_status(power_status);
}

esp_err_t smartwatch_board_power_get_event(smartwatch_board_power_event_t *event, uint32_t timeout_ms)
{
    if (!status.power_ready || power_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xQueueReceive(power_event_queue, event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE
               ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t smartwatch_board_power_off(void)
{
    if (!status.power_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (status.display_ready) {
        bsp_display_backlight_off();
    }
    ESP_RETURN_ON_ERROR(smartwatch_board_audio_stop(), TAG, "Audio shutdown failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    return smartwatch_power_driver_shutdown();
}

uint64_t smartwatch_board_get_wakeup_ext1_mask(void)
{
    return BIT64(GPIO_NUM_0);
}

gpio_num_t smartwatch_board_get_wakeup_ext0_gpio(void)
{
    return GPIO_NUM_10;
}

int smartwatch_board_get_wakeup_ext0_level(void)
{
    return 1;
}

bool smartwatch_board_touch_wakeup_pending(void)
{
    if (!status.touch_ready) {
        return false;
    }
    return gpio_get_level(SMARTWATCH_TOUCH_WAKE_GPIO) == 0;
}

esp_err_t smartwatch_board_button_get_event(smartwatch_board_button_event_t *event, uint32_t timeout_ms)
{
    if (!status.buttons_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return xQueueReceive(button_queue, event, pdMS_TO_TICKS(timeout_ms)) == pdTRUE
               ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool smartwatch_board_button_is_pressed(smartwatch_board_button_t button)
{
    if (!status.buttons_ready || button > SMARTWATCH_BUTTON_POWER) {
        return false;
    }
    return buttons[button].stable_pressed;
}
