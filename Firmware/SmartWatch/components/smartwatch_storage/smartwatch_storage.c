#include "smartwatch_storage.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "smartwatch_platform.h"

static const char *TAG = "storage";
static const char *NAMESPACE = "settings";
static smartwatch_storage_status_t status;

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Recovering incompatible NVS partition");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t mount_assets(void)
{
    const esp_vfs_spiffs_conf_t config = {
        .base_path = "/assets",
        .partition_label = "assets",
        .max_files = 8,
        .format_if_mount_failed = false,
    };
    return esp_vfs_spiffs_register(&config);
}

esp_err_t smartwatch_storage_init(void)
{
    smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_STORAGE,
                                              SMARTWATCH_SERVICE_STARTING, ESP_OK);
    esp_err_t nvs_err = init_nvs();
    status.nvs_ready = nvs_err == ESP_OK;

    esp_err_t assets_err = mount_assets();
    status.assets_ready = assets_err == ESP_OK || assets_err == ESP_ERR_INVALID_STATE;

    const smartwatch_storage_event_t event = {
        .nvs_ready = status.nvs_ready,
        .assets_ready = status.assets_ready,
    };
    smartwatch_platform_post(SMARTWATCH_EVENT_STORAGE_STATE, &event, sizeof(event), 100);

    if (!status.nvs_ready) {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_STORAGE,
                                                  SMARTWATCH_SERVICE_FAILED, nvs_err);
        return nvs_err;
    }
    if (!status.assets_ready) {
        smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_STORAGE,
                                                  SMARTWATCH_SERVICE_DEGRADED, assets_err);
        return ESP_OK;
    }
    return smartwatch_platform_publish_service_state(SMARTWATCH_SERVICE_STORAGE,
                                                     SMARTWATCH_SERVICE_READY, ESP_OK);
}

const smartwatch_storage_status_t *smartwatch_storage_get_status(void)
{
    return &status;
}

esp_err_t smartwatch_storage_get_u32(const char *key, uint32_t *value)
{
    if (!status.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NAMESPACE, NVS_READONLY, &handle), TAG, "Open NVS failed");
    esp_err_t err = nvs_get_u32(handle, key, value);
    nvs_close(handle);
    return err;
}

esp_err_t smartwatch_storage_set_u32(const char *key, uint32_t value)
{
    if (!status.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (key == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NAMESPACE, NVS_READWRITE, &handle), TAG, "Open NVS failed");
    esp_err_t err = nvs_set_u32(handle, key, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
