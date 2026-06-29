#include "service_manager.h"

#include <stddef.h>
#include <string.h>

#include "event_bus.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "logger.h"

#define MAX_SERVICES 8
#define SERVICE_START_RETRIES 3
#define SERVICE_RETRY_DELAY_MS 100
#define SERVICE_HEALTH_LOG_MS 30000
#define SERVICE_RECOVERY_POLL_MS 1000
#define SERVICE_RECOVERY_INITIAL_BACKOFF_MS 2000
#define SERVICE_RECOVERY_MAX_BACKOFF_MS 30000

static const service_t *services[MAX_SERVICES];
static service_runtime_info_t runtime[MAX_SERVICES];
static size_t service_count;
static TaskHandle_t health_task_handle;
static TaskHandle_t recovery_task_handle;

static const char *TAG = "services";

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void publish_service_state(size_t index)
{
    const event_service_t event = {
        .name = runtime[index].name,
        .state = runtime[index].state,
        .last_error = runtime[index].last_error,
        .failures = runtime[index].failure_count,
        .restarts = runtime[index].restart_count,
    };
    event_bus_publish(EVENT_SERVICE_STATE, &event, sizeof(event), 0);
}

static void set_service_state(size_t index, service_state_t state, esp_err_t err)
{
    runtime[index].state = state;
    runtime[index].last_error = err;
    if (err != ESP_OK) {
        runtime[index].failure_count++;
    }
    publish_service_state(index);
}

static void schedule_retry(size_t index)
{
    if (!runtime[index].auto_recover) {
        runtime[index].next_retry_ms = 0;
        return;
    }

    uint32_t backoff_ms = SERVICE_RECOVERY_INITIAL_BACKOFF_MS;
    if (runtime[index].restart_count > 0) {
        uint32_t shift = runtime[index].restart_count - 1;
        if (shift > 4) {
            shift = 4;
        }
        backoff_ms <<= shift;
    }
    if (backoff_ms > SERVICE_RECOVERY_MAX_BACKOFF_MS) {
        backoff_ms = SERVICE_RECOVERY_MAX_BACKOFF_MS;
    }
    runtime[index].next_retry_ms = now_ms() + backoff_ms;
}

static esp_err_t start_service(size_t index, bool is_restart)
{
    const service_t *service = services[index];
    runtime[index].start_attempts = 0;
    if (is_restart) {
        runtime[index].restart_count++;
    }

    for (uint8_t attempt = 1; attempt <= SERVICE_START_RETRIES; ++attempt) {
        runtime[index].start_attempts = attempt;
        if (service->init != NULL) {
            set_service_state(index, SERVICE_STATE_INITIALIZING, ESP_OK);
            esp_err_t err = service->init();
            if (err != ESP_OK) {
                set_service_state(index, SERVICE_STATE_DEGRADED, err);
                schedule_retry(index);
                vTaskDelay(pdMS_TO_TICKS(SERVICE_RETRY_DELAY_MS));
                continue;
            }
        }

        if (service->start != NULL) {
            set_service_state(index, SERVICE_STATE_STARTING, ESP_OK);
            esp_err_t err = service->start();
            if (err != ESP_OK) {
                set_service_state(index, SERVICE_STATE_DEGRADED, err);
                schedule_retry(index);
                vTaskDelay(pdMS_TO_TICKS(SERVICE_RETRY_DELAY_MS));
                continue;
            }
        }

        set_service_state(index, SERVICE_STATE_READY, ESP_OK);
        runtime[index].next_retry_ms = 0;
        return ESP_OK;
    }

    schedule_retry(index);
    set_service_state(index, SERVICE_STATE_FAILED, runtime[index].last_error);
    return runtime[index].last_error == ESP_OK ? ESP_FAIL : runtime[index].last_error;
}

static esp_err_t stop_service(size_t index)
{
    if (services[index]->stop == NULL) {
        return ESP_OK;
    }
    if (runtime[index].state == SERVICE_STATE_STOPPED) {
        return ESP_OK;
    }

    esp_err_t err = services[index]->stop();
    if (err != ESP_OK) {
        set_service_state(index, SERVICE_STATE_DEGRADED, err);
        schedule_retry(index);
        return err;
    }

    set_service_state(index, SERVICE_STATE_STOPPED, ESP_OK);
    return ESP_OK;
}

static void health_task(void *arg)
{
    (void)arg;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(SERVICE_HEALTH_LOG_MS));
        LOGI(TAG, "Health: heap=%" PRIu32 " min=%" PRIu32 " psram=%" PRIu32,
             esp_get_free_heap_size(), esp_get_minimum_free_heap_size(),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        for (size_t i = 0; i < service_count; ++i) {
            if (runtime[i].state == SERVICE_STATE_READY &&
                services[i]->health_check != NULL) {
                esp_err_t err = services[i]->health_check();
                if (err != ESP_OK) {
                    LOGW(TAG, "Service %s health check failed: %s",
                         runtime[i].name, esp_err_to_name(err));
                    set_service_state(i, SERVICE_STATE_DEGRADED, err);
                    schedule_retry(i);
                }
            }

            if (runtime[i].state != SERVICE_STATE_READY) {
                uint32_t retry_in_ms = 0;
                if (runtime[i].next_retry_ms > 0 && runtime[i].next_retry_ms > now_ms()) {
                    retry_in_ms = runtime[i].next_retry_ms - now_ms();
                }
                LOGW(TAG, "Service %s state=%d err=%s failures=%u restarts=%u retry_in=%" PRIu32 "ms",
                     runtime[i].name, runtime[i].state,
                     esp_err_to_name(runtime[i].last_error),
                     runtime[i].failure_count, runtime[i].restart_count, retry_in_ms);
            }
        }
    }
}

static void recovery_task(void *arg)
{
    (void)arg;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(SERVICE_RECOVERY_POLL_MS));

        const uint32_t current_ms = now_ms();
        for (size_t i = 0; i < service_count; ++i) {
            if (!runtime[i].auto_recover) {
                continue;
            }
            if (runtime[i].state != SERVICE_STATE_DEGRADED &&
                runtime[i].state != SERVICE_STATE_FAILED) {
                continue;
            }
            if (runtime[i].next_retry_ms == 0 || runtime[i].next_retry_ms > current_ms) {
                continue;
            }

            LOGW(TAG, "Recovering service %s from state=%d err=%s",
                 runtime[i].name, runtime[i].state,
                 esp_err_to_name(runtime[i].last_error));

            if (services[i]->stop != NULL) {
                stop_service(i);
            }
            start_service(i, true);
        }
    }
}

esp_err_t service_manager_init(void)
{
    service_count = 0;
    health_task_handle = NULL;
    recovery_task_handle = NULL;
    memset(runtime, 0, sizeof(runtime));
    return ESP_OK;
}

esp_err_t service_manager_register(const service_t *service)
{
    if (service == NULL || service->name == NULL || service_count >= MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    services[service_count] = service;
    runtime[service_count] = (service_runtime_info_t) {
        .name = service->name,
        .state = SERVICE_STATE_STOPPED,
        .last_error = ESP_OK,
        .has_stop = service->stop != NULL,
        .critical = service->critical,
        .auto_recover = service->auto_recover,
        .next_retry_ms = 0,
    };
    service_count++;
    return ESP_OK;
}

esp_err_t service_manager_start_all(void)
{
    esp_err_t critical_err = ESP_OK;

    for (size_t i = 0; i < service_count; ++i) {
        esp_err_t err = start_service(i, false);
        if (err != ESP_OK && runtime[i].critical && critical_err == ESP_OK) {
            critical_err = err;
        }
    }
    if (health_task_handle == NULL &&
        xTaskCreate(health_task, "svc_health", 3072, NULL, 2, &health_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    if (recovery_task_handle == NULL &&
        xTaskCreate(recovery_task, "svc_recover", 3584, NULL, 3, &recovery_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return critical_err;
}

size_t service_manager_count(void) { return service_count; }

size_t service_manager_degraded_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < service_count; ++i) {
        if (runtime[i].state == SERVICE_STATE_DEGRADED) {
            count++;
        }
    }
    return count;
}

size_t service_manager_failed_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < service_count; ++i) {
        if (runtime[i].state == SERVICE_STATE_FAILED) {
            count++;
        }
    }
    return count;
}

const service_runtime_info_t *service_manager_info(size_t index)
{
    return index < service_count ? &runtime[index] : NULL;
}

const service_runtime_info_t *service_manager_find(const char *name)
{
    if (name == NULL) return NULL;
    for (size_t i = 0; i < service_count; ++i) {
        if (strcmp(runtime[i].name, name) == 0) {
            return &runtime[i];
        }
    }
    return NULL;
}

esp_err_t service_manager_restart(const char *name)
{
    if (name == NULL) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < service_count; ++i) {
        if (strcmp(runtime[i].name, name) != 0) continue;
        if (services[i]->stop != NULL &&
            runtime[i].state != SERVICE_STATE_STOPPED) {
            esp_err_t err = stop_service(i);
            if (err != ESP_OK) {
                return err;
            }
        }
        return start_service(i, true);
    }
    return ESP_ERR_NOT_FOUND;
}
