#include "platform_imu.h"

#include "smartwatch_board.h"

esp_err_t platform_imu_read(platform_imu_sample_t *sample)
{
    if (sample == NULL) return ESP_ERR_INVALID_ARG;
    qmi8658_dev_t *imu = smartwatch_board_get_imu();
    if (imu == NULL) return ESP_ERR_INVALID_STATE;
    qmi8658_data_t value = {0};
    esp_err_t err = qmi8658_read_sensor_data(imu, &value);
    if (err != ESP_OK) return err;
    const float accel_scale = 1.0f / 1000.0f;
    *sample = (platform_imu_sample_t) {
        .accel_x = value.accelX * accel_scale,
        .accel_y = value.accelY * accel_scale,
        .accel_z = value.accelZ * accel_scale,
        .gyro_x = value.gyroX, .gyro_y = value.gyroY, .gyro_z = value.gyroZ,
        .temperature_c = value.temperature,
    };
    return ESP_OK;
}
