# bsp_obd_dsp — board support

English | [简体中文](README.md)

Hardware peripherals and communication links. Target board: Waveshare
ESP32-S3-Touch-LCD-1.85 (`bsp_board.h` is the board configuration entry;
V1/V2/V3 hardware revisions are picked under menuconfig → OBD DSP
Configuration).

| Module | Responsibility |
|--------|----------------|
| `elm327_ble_client.*` | ELM327 adapter BLE client; a **single-threaded polling state machine** — do not introduce parallel or interleaved requests |
| `espnow_link.*`, `gauge_pair_ble_client.*`, `ble_adv_util.*` | Triple-gauge link: ESP-NOW data broadcast + Bluetooth pairing |
| `lcd_driver/` (ST77916 + esp_lcd component), `touch_driver/`, `i2c_driver/`, `exio/` | Display, touch, I2C bus, IO expander |
| `nvs_storage.*` | User settings persistence; new fields in `nvs_user_cfg_t` **must only be appended at the end** (backward compatibility) |
| `ads1115_oil_pressure.*` | Oil pressure via ADS1115 I2C ADC |
| `rs485_brake_temp.*` | Brake temperature over RS485 |
| `racechrono_ble_diy.*` | BLE DIY output to the RaceChrono phone app |
