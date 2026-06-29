# Hardware

| Device | Bus / pins | Firmware owner |
| --- | --- | --- |
| CO5300 AMOLED 410x502 | QSPI GPIO 4/5/6/7/11/12, reset 8, TE 13 | `platform_display` |
| FT3168 touch | I2C GPIO 15/14, reset 9, interrupt 38 | `platform_touch` |
| AXP2101 PMIC | I2C GPIO 15/14 | `platform_power` |
| QMI8658 IMU | I2C GPIO 15/14, interrupt 21 | `platform_imu` |
| PCF85063A RTC | I2C GPIO 15/14, interrupt 39 | `platform_rtc` |
| ES8311 / ES7210 | I2S and shared I2C | `platform_audio` |
| microSD | GPIO 1/2/3/17 | `platform_storage` |
| BOOT / PWR buttons | GPIO 0/10 | `platform_input` |

Flash is 32 MB and PSRAM is 8 MB Octal at 80 MHz. Power rails and charging current must not be changed without schematic and battery validation.

The local board abstraction remains in `components/platform/smartwatch_board`. Waveshare drivers are under `components/third_party/waveshare` and should remain vendor-compatible.
