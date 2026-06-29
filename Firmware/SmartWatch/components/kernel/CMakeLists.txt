idf_component_register(
    SRCS
        "kernel.c"
        "app_manager.c"
        "event_bus.c"
        "service_manager.c"
        "screen_manager.c"
        "session_manager.c"
        "input_manager.c"
        "storage_manager.c"
        "logger.c"
    INCLUDE_DIRS "include"
    REQUIRES platform esp_event esp_timer nvs_flash spiffs
)
