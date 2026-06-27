#include "smartwatch_board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t smartwatch_board_motion_calibrate(smartwatch_board_motion_calibration_t *calibration,
                                            size_t sample_count, uint32_t sample_interval_ms)
{
    qmi8658_dev_t *imu = smartwatch_board_get_imu();
    if (calibration == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (imu == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    *calibration = (smartwatch_board_motion_calibration_t) {0};
    for (size_t i = 0; i < sample_count; ++i) {
        qmi8658_data_t sample = {0};
        esp_err_t err = qmi8658_read_sensor_data(imu, &sample);
        if (err != ESP_OK) {
            return err;
        }
        calibration->accel_x += sample.accelX;
        calibration->accel_y += sample.accelY;
        calibration->accel_z += sample.accelZ;
        calibration->gyro_x += sample.gyroX;
        calibration->gyro_y += sample.gyroY;
        calibration->gyro_z += sample.gyroZ;
        if (sample_interval_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(sample_interval_ms));
        }
    }

    const float divisor = (float)sample_count;
    calibration->accel_x /= divisor;
    calibration->accel_y /= divisor;
    calibration->accel_z /= divisor;
    calibration->gyro_x /= divisor;
    calibration->gyro_y /= divisor;
    calibration->gyro_z /= divisor;
    return ESP_OK;
}

esp_err_t smartwatch_board_motion_read(const smartwatch_board_motion_calibration_t *calibration,
                                       qmi8658_data_t *data)
{
    qmi8658_dev_t *imu = smartwatch_board_get_imu();
    if (calibration == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (imu == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = qmi8658_read_sensor_data(imu, data);
    if (err != ESP_OK) {
        return err;
    }
    data->accelX -= calibration->accel_x;
    data->accelY -= calibration->accel_y;
    data->accelZ -= calibration->accel_z;
    data->gyroX -= calibration->gyro_x;
    data->gyroY -= calibration->gyro_y;
    data->gyroZ -= calibration->gyro_z;
    return ESP_OK;
}
