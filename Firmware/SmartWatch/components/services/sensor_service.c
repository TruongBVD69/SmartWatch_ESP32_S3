#include "sensor_service.h"

#include <math.h>
#include <string.h>
#include <sys/param.h>

#include "esp_task_wdt.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "logger.h"

static const char *TAG = "sensor_service";
static platform_imu_sample_t latest;
static bool ready;
static TaskHandle_t sensor_task_handle;
static portMUX_TYPE sensor_lock = portMUX_INITIALIZER_UNLOCKED;

#define SENSOR_SAMPLE_PERIOD_MS             40
#define STEP_INTERVAL_MIN_MS                280
#define STEP_INTERVAL_MAX_MS                1500
#define STEP_CONFIRM_COUNT                  3
#define STEP_ACTIVITY_TIMEOUT_MS            2500
#define STEP_GRAVITY_ALPHA                  0.94f
#define STEP_DYNAMIC_ALPHA                  0.35f
#define STEP_ACTIVITY_ALPHA                 0.15f
#define STEP_NOISE_ALPHA                    0.02f
#define STEP_PEAK_THRESHOLD_MIN             0.12f
#define STEP_PEAK_THRESHOLD_MAX             0.45f
#define STEP_VALLEY_RESET_THRESHOLD         0.02f
#define STEP_GYRO_GATE_DPS                  12.0f
#define STEP_ACTIVITY_GATE                  0.08f
#define STEP_INTERVAL_TOLERANCE_MS          220
#define SENSOR_DEBUG_LOG_PERIOD_MS          1000

typedef struct {
    bool initialized;
    float gravity_mag;
    float dynamic_lp;
    float activity_lp;
    float noise_floor;
    float last_hp;
    float prev_hp;
    float peak_value;
    bool rising;
    bool armed;
    uint32_t candidate_count;
    uint32_t step_count;
    uint32_t last_step_ms;
    uint32_t last_motion_ms;
    uint16_t cadence_spm;
    uint8_t confidence;
    bool walking;
    uint16_t recent_intervals[4];
    uint8_t recent_count;
} pedometer_state_t;

static pedometer_state_t pedometer;
static uint32_t last_debug_log_ms;

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static uint32_t sensor_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void pedometer_reset_runtime(pedometer_state_t *state)
{
    uint32_t preserved_steps = state->step_count;
    memset(state, 0, sizeof(*state));
    state->step_count = preserved_steps;
}

static void pedometer_push_interval(pedometer_state_t *state, uint16_t interval_ms)
{
    if (state->recent_count < sizeof(state->recent_intervals) / sizeof(state->recent_intervals[0])) {
        state->recent_intervals[state->recent_count++] = interval_ms;
        return;
    }
    memmove(&state->recent_intervals[0], &state->recent_intervals[1],
            sizeof(state->recent_intervals) - sizeof(state->recent_intervals[0]));
    state->recent_intervals[state->recent_count - 1] = interval_ms;
}

static uint16_t pedometer_compute_cadence(const pedometer_state_t *state)
{
    if (state->recent_count == 0) {
        return 0;
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < state->recent_count; ++i) {
        sum += state->recent_intervals[i];
    }
    const uint32_t avg_ms = sum / state->recent_count;
    if (avg_ms == 0) {
        return 0;
    }
    return (uint16_t)(60000U / avg_ms);
}

static uint8_t pedometer_compute_confidence(const pedometer_state_t *state, float amplitude,
                                            float gyro_mag)
{
    int score = 35;
    score += MIN((int)(amplitude * 120.0f), 30);
    score += MIN((int)(gyro_mag * 0.7f), 20);

    if (state->recent_count >= 2) {
        uint16_t min_interval = state->recent_intervals[0];
        uint16_t max_interval = state->recent_intervals[0];
        for (uint8_t i = 1; i < state->recent_count; ++i) {
            min_interval = MIN(min_interval, state->recent_intervals[i]);
            max_interval = MAX(max_interval, state->recent_intervals[i]);
        }
        const uint16_t spread = max_interval - min_interval;
        if (spread < 80) score += 20;
        else if (spread < 140) score += 10;
        else score -= 10;
    }

    if (state->candidate_count >= STEP_CONFIRM_COUNT) {
        score += 15;
    }

    return (uint8_t)MIN(MAX(score, 0), 100);
}

static bool pedometer_accept_step(pedometer_state_t *state, uint32_t now_ms, float amplitude,
                                  float gyro_mag)
{
    if (state->last_step_ms != 0) {
        const uint32_t interval_ms = now_ms - state->last_step_ms;
        if (interval_ms < STEP_INTERVAL_MIN_MS || interval_ms > STEP_INTERVAL_MAX_MS) {
            state->candidate_count = 0;
            state->recent_count = 0;
            state->walking = false;
            return false;
        }

        if (state->recent_count > 0) {
            const uint16_t reference = state->recent_intervals[state->recent_count - 1];
            const uint32_t diff = (interval_ms > reference) ? (interval_ms - reference)
                                                            : (reference - interval_ms);
            if (diff > STEP_INTERVAL_TOLERANCE_MS && state->candidate_count < STEP_CONFIRM_COUNT + 1) {
                state->candidate_count = 1;
                state->recent_count = 0;
            }
        }
        pedometer_push_interval(state, (uint16_t)interval_ms);
        state->cadence_spm = pedometer_compute_cadence(state);
    }

    state->last_step_ms = now_ms;
    state->candidate_count++;

    if (state->candidate_count >= STEP_CONFIRM_COUNT) {
        if (!state->walking) {
            state->step_count += STEP_CONFIRM_COUNT;
        } else {
            state->step_count++;
        }
        state->walking = true;
        state->confidence = pedometer_compute_confidence(state, amplitude, gyro_mag);
        return true;
    }

    state->confidence = 20;
    return false;
}

static bool pedometer_process_sample(pedometer_state_t *state, const platform_imu_sample_t *sample,
                                     float accel_mag, float gyro_mag, uint32_t now_ms)
{
    if (!state->initialized) {
        state->gravity_mag = accel_mag;
        state->noise_floor = 0.03f;
        state->initialized = true;
        return false;
    }

    state->gravity_mag = STEP_GRAVITY_ALPHA * state->gravity_mag +
                         (1.0f - STEP_GRAVITY_ALPHA) * accel_mag;
    const float hp = accel_mag - state->gravity_mag;
    state->dynamic_lp = STEP_DYNAMIC_ALPHA * hp + (1.0f - STEP_DYNAMIC_ALPHA) * state->dynamic_lp;
    state->activity_lp = STEP_ACTIVITY_ALPHA * fabsf(state->dynamic_lp) +
                         (1.0f - STEP_ACTIVITY_ALPHA) * state->activity_lp;
    state->noise_floor = STEP_NOISE_ALPHA * fabsf(hp) +
                         (1.0f - STEP_NOISE_ALPHA) * state->noise_floor;

    const float peak_threshold = clampf(state->noise_floor * 3.2f + 0.04f,
                                        STEP_PEAK_THRESHOLD_MIN, STEP_PEAK_THRESHOLD_MAX);
    const bool motion_gate = state->activity_lp >= STEP_ACTIVITY_GATE || gyro_mag >= STEP_GYRO_GATE_DPS;
    if (motion_gate) {
        state->last_motion_ms = now_ms;
    } else if ((now_ms - state->last_motion_ms) > STEP_ACTIVITY_TIMEOUT_MS) {
        state->walking = false;
        state->candidate_count = 0;
        state->recent_count = 0;
        state->cadence_spm = 0;
        state->confidence = 0;
    }

    bool counted = false;
    if (state->dynamic_lp > state->prev_hp) {
        state->rising = true;
        if (state->dynamic_lp > state->peak_value) {
            state->peak_value = state->dynamic_lp;
        }
    } else if (state->rising) {
        if (state->peak_value >= peak_threshold && motion_gate && state->armed) {
            counted = pedometer_accept_step(state, now_ms, state->peak_value, gyro_mag);
        }
        state->rising = false;
        state->peak_value = 0.0f;
        state->armed = false;
    }

    if (state->dynamic_lp < -STEP_VALLEY_RESET_THRESHOLD) {
        state->armed = true;
    }

    state->last_hp = hp;
    state->prev_hp = state->dynamic_lp;
    (void)sample;
    return counted;
}

esp_err_t sensor_service_read(platform_imu_sample_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ready) {
        esp_err_t err = platform_imu_read(sample);
        if (err == ESP_OK) {
            latest = *sample;
            ready = true;
        }
        return err;
    }
    *sample = latest;
    return ESP_OK;
}

esp_err_t sensor_service_get_step_state(sensor_service_step_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    taskENTER_CRITICAL(&sensor_lock);
    *state = (sensor_service_step_state_t) {
        .step_count = pedometer.step_count,
        .cadence_spm = pedometer.cadence_spm,
        .step_confidence = pedometer.confidence,
        .walking = pedometer.walking,
    };
    taskEXIT_CRITICAL(&sensor_lock);
    return ESP_OK;
}

static void sensor_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);
    while (true) {
        esp_err_t err = platform_imu_read(&latest);
        if (err == ESP_OK) {
            ready = true;
            const float accel_mag = sqrtf(latest.accel_x * latest.accel_x +
                                          latest.accel_y * latest.accel_y +
                                          latest.accel_z * latest.accel_z);
            const float gyro_mag = sqrtf(latest.gyro_x * latest.gyro_x +
                                         latest.gyro_y * latest.gyro_y +
                                         latest.gyro_z * latest.gyro_z);
            const uint32_t now_ms = sensor_now_ms();
            taskENTER_CRITICAL(&sensor_lock);
            (void)pedometer_process_sample(&pedometer, &latest, accel_mag, gyro_mag, now_ms);
            const event_sensor_t event = {
                .temperature_c = latest.temperature_c,
                .accel_mag = accel_mag,
                .gyro_mag = gyro_mag,
                .step_count = pedometer.step_count,
                .cadence_spm = pedometer.cadence_spm,
                .step_confidence = pedometer.confidence,
                .walking = pedometer.walking,
            };
            const uint32_t step_count = pedometer.step_count;
            const uint16_t cadence_spm = pedometer.cadence_spm;
            const uint8_t confidence = pedometer.confidence;
            const bool walking = pedometer.walking;
            taskEXIT_CRITICAL(&sensor_lock);
            event_bus_publish(EVENT_SENSOR_UPDATED, &event, sizeof(event), 100);
            if ((now_ms - last_debug_log_ms) >= SENSOR_DEBUG_LOG_PERIOD_MS) {
                last_debug_log_ms = now_ms;
                LOGI(TAG, "Motion: acc=%.03fg gyro=%.2f step=%lu cad=%u conf=%u walk=%d temp=%.1fC",
                     accel_mag, gyro_mag, (unsigned long)step_count, cadence_spm,
                     confidence, walking, latest.temperature_c);
            }
        } else {
            LOGW(TAG, "IMU read failed: %s", esp_err_to_name(err));
            ready = false;
            taskENTER_CRITICAL(&sensor_lock);
            pedometer_reset_runtime(&pedometer);
            taskEXIT_CRITICAL(&sensor_lock);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS));
    }
}

static esp_err_t sensor_init(void)
{
    memset(&pedometer, 0, sizeof(pedometer));
    ready = platform_imu_read(&latest) == ESP_OK;
    return ESP_OK;
}

static esp_err_t sensor_start(void)
{
    if (sensor_task_handle != NULL) {
        return ESP_OK;
    }
    return xTaskCreate(sensor_task, "sensor", 3072, NULL, 4, &sensor_task_handle) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t sensor_stop(void)
{
    if (sensor_task_handle != NULL) {
        esp_task_wdt_delete(sensor_task_handle);
        vTaskDelete(sensor_task_handle);
        sensor_task_handle = NULL;
    }
    ready = false;
    return ESP_OK;
}

static esp_err_t sensor_health_check(void)
{
    platform_imu_sample_t sample = {0};
    return platform_imu_read(&sample);
}

const service_t *sensor_service_descriptor(void)
{
    static const service_t service = {
        .name = "sensor",
        .init = sensor_init,
        .start = sensor_start,
        .stop = sensor_stop,
        .health_check = sensor_health_check,
        .critical = false,
        .auto_recover = true,
    };
    return &service;
}
