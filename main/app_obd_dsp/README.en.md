# app_obd_dsp — application layer

English | [简体中文](README.md)

Application logic running in the main task. Data flow: PID responses received
by `bsp_obd_dsp/elm327_ble_client` are written into `obd_data_cache`, and the
UI screens under `export_path/` read the cache to render.

| File | Responsibility |
|------|----------------|
| `obd_data_cache.*` | Single home for all live data (value, freshness, unit) |
| `vehicle_profiles.*`, `vehicle_custom_config.h` | Per-vehicle PID lists and decoding; the vehicle table source of truth is [docs/VEHICLES.en.md](../../docs/VEHICLES.en.md) |
| `app_event.*` | Cross-task event queue |
| `ota_wifi_server.*`, `ota_update_ble.*` | Dual-path OTA: WiFi HTTP endpoints + BLE transport; protocol in [docs/APP_PROTOCOL.en.md](../../docs/APP_PROTOCOL.en.md) |
| `boot_media_mount.*`, `boot_block_player.*` | Boot animation: bootmedia partition (SPIFFS) mounting + delta-frame decoding/playback |
| `theme_mount.*` | theme_0 partition mounting, works with [theme_engine](../theme_engine/README.en.md) |
| `device_identity.*` | Device manifest (the app compares it for hardware compatibility) |

Build and architecture details: [docs/DEVELOPMENT.en.md](../../docs/DEVELOPMENT.en.md).
