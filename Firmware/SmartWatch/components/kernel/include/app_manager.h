#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    APP_ID_WATCHFACE = 0,
    APP_ID_LAUNCHER,
    APP_ID_SETTINGS,
    APP_ID_DIAGNOSTICS,
    APP_ID_XIAOZHI,
    APP_ID_MAX
} app_id_t;

typedef struct {
    app_id_t id;
    const char *name;
    esp_err_t (*init)(void);
    esp_err_t (*enter)(void *param);
    esp_err_t (*exit)(void);
    esp_err_t (*pause)(void);
    esp_err_t (*resume)(void);
} app_t;

esp_err_t app_manager_init(void);
esp_err_t app_manager_register(const app_t *app);
esp_err_t app_manager_start(app_id_t id);
esp_err_t app_manager_open(app_id_t id, void *param);
esp_err_t app_manager_open_root(app_id_t id, void *param);
esp_err_t app_manager_back(void);
bool app_manager_can_go_back(void);
const app_t *app_manager_current(void);
size_t app_manager_history_depth(void);
const app_t *app_manager_history_peek(size_t index_from_top);
