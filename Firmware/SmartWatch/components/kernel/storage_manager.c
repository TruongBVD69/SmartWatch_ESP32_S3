#include "storage_manager.h"

#include "esp_check.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "storage_manager";
static storage_manager_status_t status;

esp_err_t storage_manager_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS recovery erase failed");
        err = nvs_flash_init();
    }
    status.nvs_ready = err == ESP_OK;
    if (!status.nvs_ready) return err;

    const esp_vfs_spiffs_conf_t config = {
        .base_path = "/assets",
        .partition_label = "assets",
        .max_files = 8,
        .format_if_mount_failed = true,
    };
    err = esp_vfs_spiffs_register(&config);
    status.assets_ready = err == ESP_OK || err == ESP_ERR_INVALID_STATE;
    if (!status.assets_ready) {
        ESP_LOGW(TAG, "Assets mount unavailable: %s", esp_err_to_name(err));
    }
    return ESP_OK;
}

const storage_manager_status_t *storage_manager_status(void) { return &status; }

esp_err_t storage_manager_get_u32(const char *key, uint32_t *value)
{
    if (!status.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (key == NULL || value == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("settings", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "Open NVS failed");
    err = nvs_get_u32(handle, key, value);
    nvs_close(handle);
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
}

esp_err_t storage_manager_set_u32(const char *key, uint32_t value)
{
    if (!status.nvs_ready) return ESP_ERR_INVALID_STATE;
    if (key == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open("settings", NVS_READWRITE, &handle), TAG, "Open NVS failed");
    esp_err_t err = nvs_set_u32(handle, key, value);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
