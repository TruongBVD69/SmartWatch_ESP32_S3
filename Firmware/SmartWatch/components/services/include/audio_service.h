#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "platform_audio.h"
#include "service_manager.h"

esp_err_t audio_service_start(const platform_audio_config_t *config, bool speaker, bool microphone);
esp_err_t audio_service_stop(void);
esp_err_t audio_service_read(void *data, size_t size);
esp_err_t audio_service_write(const void *data, size_t size);
esp_err_t audio_service_set_volume(float percent);
esp_err_t audio_service_set_input_gain(float gain_db);
const service_t *audio_service_descriptor(void);
