# Waveshare Components Used by SmartWatch

This directory is a trimmed vendor snapshot. It intentionally contains only
the Waveshare components required by this board:

- `bsp/esp32_s3_touch_amoled_2_06`
- `sensor/qmi8658`
- `sensor/pcf85063a`

Display, touch, LVGL, audio codec, and PMIC drivers are resolved through the
ESP-IDF Component Manager. Their versions are declared in
`../smartwatch_board/idf_component.yml` and locked in the project-level
`dependencies.lock`.

Do not replace this directory with the complete upstream repository during an
upgrade. Update each retained component independently and run the hardware
smoke tests documented in `docs/HARDWARE.md`.
