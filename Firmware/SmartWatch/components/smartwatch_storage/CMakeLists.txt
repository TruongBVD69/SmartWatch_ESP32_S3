idf_component_register(
    SRCS "smartwatch_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash spiffs smartwatch_platform
)
