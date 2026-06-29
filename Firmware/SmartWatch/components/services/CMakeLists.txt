idf_component_register(
    SRCS
        "services.c"
        "battery_service.c"
        "time_service.c"
        "sensor_service.c"
        "audio_service.c"
        "storage_service.c"
    INCLUDE_DIRS "include"
    REQUIRES platform kernel esp_system
)
