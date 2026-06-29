# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/platform"
    "${CMAKE_CURRENT_LIST_DIR}/components/platform/smartwatch_board"
    "${CMAKE_CURRENT_LIST_DIR}/components/kernel"
    "${CMAKE_CURRENT_LIST_DIR}/components/services"
    "${CMAKE_CURRENT_LIST_DIR}/components/ui"
    "${CMAKE_CURRENT_LIST_DIR}/components/apps"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/esp_lcd_touch_ft3168"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/waveshare/bsp/esp32_s3_touch_amoled_2_06"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/waveshare/sensor/qmi8658"
    "${CMAKE_CURRENT_LIST_DIR}/components/third_party/waveshare/sensor/pcf85063a"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(SmartWatch)
