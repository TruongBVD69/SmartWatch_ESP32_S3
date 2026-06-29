#include "platform_audio.h"

#include "smartwatch_board.h"

esp_err_t platform_audio_start(const platform_audio_config_t *config, bool speaker, bool microphone)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    const smartwatch_board_audio_config_t board_config = {
        .sample_rate_hz = config->sample_rate_hz,
        .channels = config->channels,
        .bits_per_sample = config->bits_per_sample,
    };
    return smartwatch_board_audio_start(&board_config, speaker, microphone);
}

esp_err_t platform_audio_stop(void) { return smartwatch_board_audio_stop(); }
esp_err_t platform_audio_write(const void *data, size_t size) { return smartwatch_board_audio_write(data, size); }
esp_err_t platform_audio_read(void *data, size_t size) { return smartwatch_board_audio_read(data, size); }
esp_err_t platform_audio_set_volume(float percent) { return smartwatch_board_audio_set_volume(percent); }
esp_err_t platform_audio_set_input_gain(float gain_db) { return smartwatch_board_audio_set_input_gain(gain_db); }
