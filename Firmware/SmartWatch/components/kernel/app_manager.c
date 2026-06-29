#include "app_manager.h"

#include <stdbool.h>
#include <stddef.h>

#include "event_bus.h"

#define APP_HISTORY_DEPTH 8

static const app_t *registry[APP_ID_MAX];
static const app_t *current;
static app_id_t history[APP_HISTORY_DEPTH];
static size_t history_depth;

static void publish_app_event(const app_t *app, event_app_state_kind_t state)
{
    if (app == NULL) return;
    const event_app_t event = {
        .app_id = app->id,
        .state = state,
    };
    event_bus_publish(EVENT_APP_STATE, &event, sizeof(event), 0);
}

static void history_push(app_id_t id)
{
    if (history_depth > 0 && history[history_depth - 1] == id) {
        return;
    }
    if (history_depth < APP_HISTORY_DEPTH) {
        history[history_depth++] = id;
        return;
    }
    for (size_t i = 1; i < APP_HISTORY_DEPTH; ++i) {
        history[i - 1] = history[i];
    }
    history[APP_HISTORY_DEPTH - 1] = id;
}

static void history_remove(app_id_t id)
{
    size_t write = 0;
    for (size_t read = 0; read < history_depth; ++read) {
        if (history[read] != id) {
            history[write++] = history[read];
        }
    }
    history_depth = write;
}

static const app_t *history_pop_valid(void)
{
    while (history_depth > 0) {
        app_id_t id = history[--history_depth];
        if (id >= 0 && id < APP_ID_MAX && registry[id] != NULL) {
            return registry[id];
        }
    }
    return NULL;
}

static esp_err_t app_enter(const app_t *app, void *param, bool resumed)
{
    if (app == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    if (resumed && app->resume != NULL) {
        err = app->resume();
    } else {
        err = app->enter(param);
    }
    if (err == ESP_OK) {
        publish_app_event(app, resumed ? EVENT_APP_STATE_RESUMED : EVENT_APP_STATE_ENTERED);
    }
    return err;
}

static esp_err_t app_leave(const app_t *app, bool exiting)
{
    if (app == NULL) {
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;
    if (exiting) {
        if (app->exit != NULL) {
            err = app->exit();
        }
        if (err == ESP_OK) {
            publish_app_event(app, EVENT_APP_STATE_EXITED);
        }
        return err;
    }

    if (app->pause != NULL) {
        err = app->pause();
    }
    if (err == ESP_OK) {
        publish_app_event(app, EVENT_APP_STATE_PAUSED);
    }
    return err;
}

esp_err_t app_manager_init(void)
{
    current = NULL;
    history_depth = 0;
    for (size_t i = 0; i < APP_ID_MAX; ++i) registry[i] = NULL;
    return ESP_OK;
}

esp_err_t app_manager_register(const app_t *app)
{
    if (app == NULL || app->id < 0 || app->id >= APP_ID_MAX || app->enter == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (registry[app->id] != NULL) return ESP_ERR_INVALID_STATE;
    if (app->init != NULL) {
        esp_err_t err = app->init();
        if (err != ESP_OK) return err;
    }
    registry[app->id] = app;
    return ESP_OK;
}

esp_err_t app_manager_open(app_id_t id, void *param)
{
    if (id < 0 || id >= APP_ID_MAX || registry[id] == NULL) return ESP_ERR_NOT_FOUND;
    const app_t *next = registry[id];
    if (current == next) return ESP_OK;

    const app_t *previous = current;
    if (previous != NULL) {
        esp_err_t err = app_leave(previous, false);
        if (err != ESP_OK) return err;
    }

    history_remove(next->id);
    esp_err_t err = app_enter(next, param, false);
    if (err != ESP_OK) {
        if (previous != NULL) {
            app_enter(previous, NULL, previous->resume != NULL);
        }
        return err;
    }

    if (previous != NULL) {
        history_push(previous->id);
    }
    current = next;
    return err;
}

esp_err_t app_manager_start(app_id_t id) { return app_manager_open(id, NULL); }

esp_err_t app_manager_open_root(app_id_t id, void *param)
{
    if (id < 0 || id >= APP_ID_MAX || registry[id] == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    const app_t *next = registry[id];
    if (current == next && history_depth == 0) {
        return ESP_OK;
    }

    if (current != NULL) {
        esp_err_t err = app_leave(current, true);
        if (err != ESP_OK) {
            return err;
        }
    }

    history_depth = 0;
    current = NULL;
    esp_err_t err = app_enter(next, param, false);
    if (err != ESP_OK) {
        return err;
    }
    current = next;
    return ESP_OK;
}

esp_err_t app_manager_back(void)
{
    if (history_depth == 0 || current == NULL) return ESP_ERR_NOT_FOUND;

    const app_t *outgoing = current;
    const app_t *previous = history_pop_valid();
    if (previous == NULL) return ESP_ERR_NOT_FOUND;

    esp_err_t err = app_leave(outgoing, true);
    if (err != ESP_OK) return err;

    err = app_enter(previous, NULL, previous->resume != NULL);
    if (err != ESP_OK) return err;

    current = previous;
    return ESP_OK;
}

bool app_manager_can_go_back(void) { return history_depth > 0; }
const app_t *app_manager_current(void) { return current; }
size_t app_manager_history_depth(void) { return history_depth; }

const app_t *app_manager_history_peek(size_t index_from_top)
{
    if (index_from_top >= history_depth) {
        return NULL;
    }
    return registry[history[history_depth - 1 - index_from_top]];
}
