#include "event_bus.h"

#include "freertos/FreeRTOS.h"

ESP_EVENT_DEFINE_BASE(SMARTWATCH_EVENTS);

static bool initialized;

esp_err_t event_bus_init(void)
{
    if (initialized) return ESP_OK;
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    initialized = true;
    return ESP_OK;
}

esp_err_t event_bus_publish(event_id_t id, const void *data, size_t size, uint32_t timeout_ms)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    return esp_event_post(SMARTWATCH_EVENTS, id, data, size, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t event_bus_subscribe(event_id_t id, event_bus_handler_t handler, void *arg)
{
    if (!initialized || handler == NULL) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_register(SMARTWATCH_EVENTS, id, handler, arg);
}

esp_err_t event_bus_unsubscribe(event_id_t id, event_bus_handler_t handler)
{
    if (!initialized || handler == NULL) return ESP_ERR_INVALID_STATE;
    return esp_event_handler_unregister(SMARTWATCH_EVENTS, id, handler);
}
