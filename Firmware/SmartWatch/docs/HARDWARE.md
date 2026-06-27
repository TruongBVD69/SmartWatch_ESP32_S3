# SmartWatch Hardware Support

## Board

- MCU: ESP32-S3R8
- Flash: 32 MB, DIO at 80 MHz
- PSRAM: 8 MB Octal at 80 MHz
- Shared I2C bus: SDA GPIO15, SCL GPIO14

## Driver Matrix

| Device | Interface | Pins | Component | State |
| --- | --- | --- | --- | --- |
| CO5300 AMOLED | QSPI | D0-D3 4/5/6/7, SCLK 11, CS 12, reset 8, TE 13 | `espressif/esp_lcd_co5300` | Integrated |
| FT3168 touch | I2C | SDA 15, SCL 14, reset 9, interrupt 38 | Local `esp_lcd_touch_ft3168` | Integrated |
| QMI8658 IMU | I2C | SDA 15, SCL 14, interrupt 21 | Waveshare `qmi8658` 2.0.0 local override | Integrated; registry 1.0.1 lacks the IDF 6 I2C dependency |
| PCF85063A RTC | I2C | SDA 15, SCL 14, interrupt 39 | Waveshare `pcf85063a` 2.0.0 local override | Integrated; registry 1.1.1 lacks the IDF 6 I2C dependency |
| AXP2101 PMIC | I2C | SDA 15, SCL 14 | `cube32esp/xpowerslib` | Battery/VBUS/system telemetry and controlled shutdown |
| ES8311 speaker codec | I2C + I2S | I2C 15/14, MCLK 16, SCLK 41, LRCK 45, DOUT 40, PA 46 | `espressif/esp_codec_dev` | BSP API available |
| ES7210 microphone ADC | I2C + I2S | I2C 15/14, ASDO 42, SCLK 41, LRCK 45, MCLK 16 | `espressif/esp_codec_dev` | BSP API available |
| microSD | SDMMC 1-bit | CMD 1, CLK 2, D0 3 | ESP-IDF `fatfs`/`sdmmc` | BSP API available |
| BOOT button | GPIO | 0 | `smartwatch_board` | Debounced button events |
| PWR signal | GPIO | 10 | `smartwatch_board` | Debounced button events; PMIC shutdown API available |

## Architecture Rules

- Application code depends only on `smartwatch_board`, never on vendor BSP headers.
- Registry dependencies are declared in `components/smartwatch_board/idf_component.yml` and resolved in `dependencies.lock`.
- Board pins and electrical policy belong in `smartwatch_board`; feature code belongs in separate components.
- PMIC rail voltages must not be changed until verified against the board schematic and a physical board revision.
- Audio, SD, radio, and other high-cost peripherals initialize on demand, not during board boot.

## Flash Layout

| Partition | Size | Purpose |
| --- | ---: | --- |
| `nvs` | 64 KB | Persistent settings and credentials |
| `otadata` | 8 KB | Active OTA slot metadata |
| `phy_init` | 4 KB | Radio PHY calibration data |
| `coredump` | 256 KB | Post-crash diagnostics |
| `ota_0` | 6 MB | Primary application image |
| `ota_1` | 6 MB | OTA update application image |
| `assets` | 19.625 MB | SPIFFS UI assets and application data |

The board API owns codec instances, I2S channels, PMIC access, and debounced button state. Applications should use `smartwatch_board_audio_start()`, the stream read/write APIs, and `smartwatch_board_audio_stop()` instead of retaining raw codec handles. Use `smartwatch_board_power_off()` for controlled shutdown.

Power state is sampled every 500 ms and exposed as queued battery, VBUS, and charging events through `smartwatch_board_power_get_event()`. The Waveshare PMIC example also polls status; the board documentation does not expose a dedicated AXP2101 IRQ GPIO.

For motion-driven applications, keep the watch still while calling `smartwatch_board_motion_calibrate()`, then use `smartwatch_board_motion_read()` for baseline-corrected accelerometer and gyroscope samples. Calibration belongs in the application lifecycle and should be repeated when the physical orientation changes.

## Upgrade Process

1. Update one dependency constraint at a time.
2. Run `idf.py fullclean build` and inspect `dependencies.lock`.
3. Run display, touch, IMU, RTC, audio, SD, and battery smoke tests on hardware.
4. Commit the manifest and lockfile together.
