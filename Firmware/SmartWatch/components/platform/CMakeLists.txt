idf_component_register(
    SRCS
        "platform.c"
        "platform_display.c"
        "platform_touch.c"
        "platform_input.c"
        "platform_audio.c"
        "platform_power.c"
        "platform_rtc.c"
        "platform_imu.c"
        "platform_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES smartwatch_board esp_hw_support
)
