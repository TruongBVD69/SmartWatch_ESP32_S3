#include "storage_service.h"

#include "platform_storage.h"

static bool mounted;

esp_err_t storage_service_mount_sd(void)
{
    if (mounted) return ESP_OK;
    esp_err_t err = platform_storage_sd_mount();
    if (err == ESP_OK) mounted = true;
    return err;
}

esp_err_t storage_service_unmount_sd(void)
{
    if (!mounted) return ESP_OK;
    esp_err_t err = platform_storage_sd_unmount();
    if (err == ESP_OK) mounted = false;
    return err;
}

bool storage_service_sd_is_mounted(void) { return mounted; }

const service_t *storage_service_descriptor(void)
{
    static const service_t service = {
        .name = "storage",
        .critical = false,
        .auto_recover = false,
    };
    return &service;
}
