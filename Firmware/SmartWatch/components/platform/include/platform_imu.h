#pragma once

#include "esp_err.h"

typedef struct {
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float temperature_c;
} platform_imu_sample_t;

esp_err_t platform_imu_read(platform_imu_sample_t *sample);
