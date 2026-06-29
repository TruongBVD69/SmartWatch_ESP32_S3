#include "platform_storage.h"

#include "smartwatch_board.h"

esp_err_t platform_storage_sd_mount(void) { return smartwatch_board_sdcard_mount(); }
esp_err_t platform_storage_sd_unmount(void) { return smartwatch_board_sdcard_unmount(); }
