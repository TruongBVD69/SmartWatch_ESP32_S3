#include "audio_service.h"

#include <stdbool.h>

static bool active;

esp_err_t audio_service_start(const platform_audio_config_t *config, bool speaker, bool microphone)
{
    esp_err_t err = platform_audio_start(config, speaker, microphone);
    if (err == ESP_OK) {
        active = true;
    }
    return err;
}

esp_err_t audio_service_stop(void)
{
    esp_err_t err = platform_audio_stop();
    if (err == ESP_OK) {
        active = false;
    }
    return err;
}

esp_err_t audio_service_read(void *data, size_t size) { return platform_audio_read(data, size); }
esp_err_t audio_service_write(const void *data, size_t size) { return platform_audio_write(data, size); }
esp_err_t audio_service_set_volume(float percent) { return platform_audio_set_volume(percent); }
esp_err_t audio_service_set_input_gain(float gain_db) { return platform_audio_set_input_gain(gain_db); }

const service_t *audio_service_descriptor(void)
{
    static const service_t service = {
        .name = "audio",
        .stop = audio_service_stop,
        .critical = false,
        .auto_recover = false,
    };
    return &service;
}
