#include "esp_lcd_touch_ft3168.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FT3168";

enum {
    FT3168_REG_TOUCH_COUNT = 0x02,
    FT3168_REG_TOUCH_DATA = 0x03,
    FT3168_REG_DEVICE_ID = 0xA0,
    FT3168_REG_POWER_MODE = 0xA5,
    FT3168_DEVICE_ID = 0x03,
    FT3168_POWER_ACTIVE = 0x00,
};

static esp_err_t ft3168_read(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t *data, size_t len)
{
    return esp_lcd_panel_io_rx_param(tp->io, reg, data, len);
}

static esp_err_t ft3168_write(esp_lcd_touch_handle_t tp, uint8_t reg, uint8_t value)
{
    return esp_lcd_panel_io_tx_param(tp->io, reg, &value, 1);
}

static void ft3168_clear_points(esp_lcd_touch_handle_t tp)
{
    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);
}

static esp_err_t ft3168_read_data(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "invalid handle");

    uint8_t count = 0;
    esp_err_t err = ft3168_read(tp, FT3168_REG_TOUCH_COUNT, &count, 1);
    if (err != ESP_OK) {
        ft3168_clear_points(tp);
        ESP_LOGW(TAG, "Touch read failed: %s", esp_err_to_name(err));
        return ESP_OK;
    }

    count &= 0x0f;
    if (count == 0 || count > 2) {
        ft3168_clear_points(tp);
        return ESP_OK;
    }

    uint8_t data[12];
    err = ft3168_read(tp, FT3168_REG_TOUCH_DATA, data, 6 * count);
    if (err != ESP_OK) {
        ft3168_clear_points(tp);
        ESP_LOGW(TAG, "Coordinate read failed: %s", esp_err_to_name(err));
        return ESP_OK;
    }

    portENTER_CRITICAL(&tp->data.lock);
    tp->data.points = count;
    for (size_t i = 0; i < count; ++i) {
        const size_t offset = i * 6;
        tp->data.coords[i].x = ((uint16_t)(data[offset] & 0x0f) << 8) | data[offset + 1];
        tp->data.coords[i].y = ((uint16_t)(data[offset + 2] & 0x0f) << 8) | data[offset + 3];
        tp->data.coords[i].track_id = data[offset + 2] >> 4;
    }
    portEXIT_CRITICAL(&tp->data.lock);
    return ESP_OK;
}

static bool ft3168_get_xy(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y,
                          uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    ESP_RETURN_ON_FALSE(tp && x && y && point_num && max_point_num, false, TAG, "invalid argument");

    portENTER_CRITICAL(&tp->data.lock);
    *point_num = tp->data.points > max_point_num ? max_point_num : tp->data.points;
    for (size_t i = 0; i < *point_num; ++i) {
        x[i] = tp->data.coords[i].x;
        y[i] = tp->data.coords[i].y;
        if (strength) {
            strength[i] = 0;
        }
    }
    tp->data.points = 0;
    portEXIT_CRITICAL(&tp->data.lock);
    return *point_num > 0;
}

static esp_err_t ft3168_del(esp_lcd_touch_handle_t tp)
{
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_INVALID_ARG, TAG, "invalid handle");
    if (tp->config.int_gpio_num != GPIO_NUM_NC) {
        if (tp->config.interrupt_callback) {
            gpio_isr_handler_remove(tp->config.int_gpio_num);
        }
        gpio_reset_pin(tp->config.int_gpio_num);
    }
    if (tp->config.rst_gpio_num != GPIO_NUM_NC) {
        gpio_reset_pin(tp->config.rst_gpio_num);
    }
    free(tp);
    return ESP_OK;
}

static esp_err_t ft3168_reset(esp_lcd_touch_handle_t tp)
{
    if (tp->config.rst_gpio_num == GPIO_NUM_NC) {
        return ESP_OK;
    }

    const int active = tp->config.levels.reset;
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !active), TAG, "reset high failed");
    vTaskDelay(pdMS_TO_TICKS(1));
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, active), TAG, "reset low failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(tp->config.rst_gpio_num, !active), TAG, "reset release failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

esp_err_t esp_lcd_touch_new_i2c_ft3168(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t *config,
                                       esp_lcd_touch_handle_t *out_touch)
{
    ESP_RETURN_ON_FALSE(io && config && out_touch, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    esp_lcd_touch_handle_t tp = calloc(1, sizeof(esp_lcd_touch_t));
    ESP_RETURN_ON_FALSE(tp, ESP_ERR_NO_MEM, TAG, "no memory");

    tp->io = io;
    tp->read_data = ft3168_read_data;
    tp->get_xy = ft3168_get_xy;
    tp->del = ft3168_del;
    tp->data.lock.owner = portMUX_FREE_VAL;
    memcpy(&tp->config, config, sizeof(*config));

    esp_err_t err = ESP_OK;
    if (config->int_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t int_config = {
            .pin_bit_mask = BIT64(config->int_gpio_num),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .intr_type = config->levels.interrupt ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE,
        };
        err = gpio_config(&int_config);
        if (err != ESP_OK) {
            goto fail;
        }
        if (config->interrupt_callback) {
            err = esp_lcd_touch_register_interrupt_callback(tp, config->interrupt_callback);
            if (err != ESP_OK) {
                goto fail;
            }
        }
    }

    if (config->rst_gpio_num != GPIO_NUM_NC) {
        const gpio_config_t reset_config = {
            .pin_bit_mask = BIT64(config->rst_gpio_num),
            .mode = GPIO_MODE_OUTPUT,
        };
        err = gpio_config(&reset_config);
        if (err != ESP_OK) {
            goto fail;
        }
    }

    if ((err = ft3168_reset(tp)) != ESP_OK) {
        goto fail;
    }

    uint8_t device_id = 0;
    if ((err = ft3168_read(tp, FT3168_REG_DEVICE_ID, &device_id, 1)) != ESP_OK) {
        goto fail;
    }
    if (device_id != FT3168_DEVICE_ID) {
        ESP_LOGW(TAG, "Unexpected device ID 0x%02X (expected 0x%02X)", device_id, FT3168_DEVICE_ID);
    }
    if ((err = ft3168_write(tp, FT3168_REG_POWER_MODE, FT3168_POWER_ACTIVE)) != ESP_OK) {
        goto fail;
    }

    ESP_LOGI(TAG, "Initialized, device ID 0x%02X, active mode", device_id);
    *out_touch = tp;
    return ESP_OK;

fail:
    ESP_LOGE(TAG, "Initialization failed: %s", esp_err_to_name(err));
    ft3168_del(tp);
    return err;
}
