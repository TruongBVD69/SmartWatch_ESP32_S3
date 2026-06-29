idf_component_register(
    SRCS "smartwatch_board.c" "smartwatch_audio.c" "smartwatch_motion.c" "smartwatch_power.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp32_s3_touch_amoled_2_06 qmi8658 pcf85063a cube32esp__xpowerslib
)
