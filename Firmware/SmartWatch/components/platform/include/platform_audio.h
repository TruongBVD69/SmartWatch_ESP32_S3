#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t channels;
    uint8_t bits_per_sample;
} platform_audio_config_t;

#define PLATFORM_AUDIO_CONFIG_DEFAULT() \
    { .sample_rate_hz = 16000, .channels = 1, .bits_per_sample = 16 }

esp_err_t platform_audio_start(const platform_audio_config_t *config, bool speaker, bool microphone);
esp_err_t platform_audio_stop(void);
esp_err_t platform_audio_write(const void *data, size_t size);
esp_err_t platform_audio_read(void *data, size_t size);
esp_err_t platform_audio_set_volume(float percent);
esp_err_t platform_audio_set_input_gain(float gain_db);
