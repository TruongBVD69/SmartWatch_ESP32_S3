#include "smartwatch_board.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t audio_mutex;
static esp_codec_dev_handle_t speaker;
static esp_codec_dev_handle_t microphone;
static bool speaker_open;
static bool microphone_open;

static esp_err_t audio_lock(void)
{
    if (audio_mutex == NULL) {
        audio_mutex = xSemaphoreCreateMutex();
        if (audio_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return xSemaphoreTake(audio_mutex, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

static esp_codec_dev_sample_info_t make_format(const smartwatch_board_audio_config_t *config)
{
    return (esp_codec_dev_sample_info_t) {
        .sample_rate = config->sample_rate_hz,
        .channel = config->channels,
        .bits_per_sample = config->bits_per_sample,
    };
}

esp_err_t smartwatch_board_audio_start(const smartwatch_board_audio_config_t *config,
                                       bool enable_speaker, bool enable_microphone)
{
    if (config == NULL || (!enable_speaker && !enable_microphone) || config->sample_rate_hz == 0 ||
        config->channels == 0 || config->bits_per_sample == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    if (speaker_open || microphone_open) {
        xSemaphoreGive(audio_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    esp_codec_dev_sample_info_t format = make_format(config);
    esp_err_t result = ESP_OK;
    if (enable_speaker) {
        speaker = bsp_audio_codec_speaker_init();
        if (speaker == NULL || esp_codec_dev_open(speaker, &format) != ESP_CODEC_DEV_OK) {
            result = ESP_FAIL;
            goto cleanup;
        }
        speaker_open = true;
    }
    if (enable_microphone) {
        microphone = bsp_audio_codec_microphone_init();
        if (microphone == NULL || esp_codec_dev_open(microphone, &format) != ESP_CODEC_DEV_OK) {
            result = ESP_FAIL;
            goto cleanup;
        }
        microphone_open = true;
    }
    xSemaphoreGive(audio_mutex);
    return ESP_OK;

cleanup:
    if (microphone_open) {
        esp_codec_dev_close(microphone);
        microphone_open = false;
    }
    if (speaker_open) {
        esp_codec_dev_close(speaker);
        speaker_open = false;
    }
    bsp_audio_deinit();
    speaker = NULL;
    microphone = NULL;
    xSemaphoreGive(audio_mutex);
    return result;
}

esp_err_t smartwatch_board_audio_stop(void)
{
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    esp_err_t result = ESP_OK;
    if (microphone_open && esp_codec_dev_close(microphone) != ESP_CODEC_DEV_OK) {
        result = ESP_FAIL;
    }
    if (speaker_open && esp_codec_dev_close(speaker) != ESP_CODEC_DEV_OK) {
        result = ESP_FAIL;
    }
    microphone_open = false;
    speaker_open = false;
    if (bsp_audio_deinit() != ESP_OK) {
        result = ESP_FAIL;
    }
    speaker = NULL;
    microphone = NULL;
    xSemaphoreGive(audio_mutex);
    return result;
}

esp_err_t smartwatch_board_audio_write(const void *data, size_t size)
{
    if (data == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (speaker_open) {
        result = esp_codec_dev_write(speaker, (void *)data, size) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGive(audio_mutex);
    return result;
}

esp_err_t smartwatch_board_audio_read(void *data, size_t size)
{
    if (data == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (microphone_open) {
        result = esp_codec_dev_read(microphone, data, size) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGive(audio_mutex);
    return result;
}

esp_err_t smartwatch_board_audio_set_volume(float volume_percent)
{
    if (volume_percent < 0.0f || volume_percent > 100.0f) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (speaker_open) {
        result = esp_codec_dev_set_out_vol(speaker, volume_percent) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGive(audio_mutex);
    return result;
}

esp_err_t smartwatch_board_audio_set_input_gain(float gain_db)
{
    ESP_RETURN_ON_ERROR(audio_lock(), "smartwatch_audio", "Audio lock failed");
    esp_err_t result = ESP_ERR_INVALID_STATE;
    if (microphone_open) {
        result = esp_codec_dev_set_in_gain(microphone, gain_db) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
    xSemaphoreGive(audio_mutex);
    return result;
}
