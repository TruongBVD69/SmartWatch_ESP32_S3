#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool display_ready;
    bool touch_ready;
    bool imu_ready;
    bool rtc_ready;
    bool power_ready;
    bool buttons_ready;
} platform_status_t;

esp_err_t platform_init(void);
const platform_status_t *platform_get_status(void);

#ifdef __cplusplus
}
#endif
