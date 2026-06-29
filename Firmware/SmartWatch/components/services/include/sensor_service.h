#pragma once

#include "esp_err.h"
#include "platform_imu.h"
#include "service_manager.h"

typedef struct {
    uint32_t step_count;
    uint16_t cadence_spm;
    uint8_t step_confidence;
    bool walking;
} sensor_service_step_state_t;

esp_err_t sensor_service_read(platform_imu_sample_t *sample);
esp_err_t sensor_service_get_step_state(sensor_service_step_state_t *state);
const service_t *sensor_service_descriptor(void);
