# SmartWatch OS

Mini smartwatch operating environment for the Waveshare ESP32-S3-Touch-AMOLED-2.06.

- ESP-IDF 6.0.1
- ESP32-S3, 32 MB flash, 8 MB Octal PSRAM
- CO5300 410x502 AMOLED with LVGL 9.5
- FT3168 touch, AXP2101 PMIC, PCF85063A RTC, QMI8658 IMU
- ES8311 speaker, ES7210 microphone, microSD

Applications depend on services and kernel APIs. Only platform code owns board and vendor drivers. See `docs/architecture.md`.

```bash
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```
