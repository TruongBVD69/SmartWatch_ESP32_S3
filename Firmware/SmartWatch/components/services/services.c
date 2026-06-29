#include "services.h"

#include "audio_service.h"
#include "battery_service.h"
#include "sensor_service.h"
#include "service_manager.h"
#include "storage_service.h"
#include "time_service.h"

esp_err_t services_init(void)
{
    const service_t *services[] = {
        battery_service_descriptor(), time_service_descriptor(), sensor_service_descriptor(),
        audio_service_descriptor(), storage_service_descriptor(),
    };
    for (size_t i = 0; i < sizeof(services) / sizeof(services[0]); ++i) {
        esp_err_t err = service_manager_register(services[i]);
        if (err != ESP_OK) return err;
    }
    return service_manager_start_all();
}
