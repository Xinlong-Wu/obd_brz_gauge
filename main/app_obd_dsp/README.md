# app_obd_dsp — 应用层

[English](README.en.md) | 简体中文

跑在主任务里的应用逻辑。数据流：`bsp_obd_dsp/elm327_ble_client` 收到的 PID
响应写入 `obd_data_cache`，`export_path/` 的 UI 页面按需读缓存渲染。

| 文件 | 职责 |
|------|------|
| `obd_data_cache.*` | 全部实时数据的唯一存放点（值、新鲜度、单位）|
| `vehicle_profiles.*`、`vehicle_custom_config.h` | 各车型的 PID 清单与解码；车型表事实源是 [docs/VEHICLES.md](../../docs/VEHICLES.md) |
| `app_event.*` | 跨任务事件队列 |
| `ota_wifi_server.*`、`ota_update_ble.*` | 双路 OTA：WiFi HTTP 端点 + BLE 传输，协议见 [docs/APP_PROTOCOL.md](../../docs/APP_PROTOCOL.md) |
| `boot_media_mount.*`、`boot_block_player.*` | 开机动画：bootmedia 分区（SPIFFS）挂载 + delta 帧解码播放 |
| `theme_mount.*` | theme_0 分区挂载，配合 [theme_engine](../theme_engine/README.md) |
| `device_identity.*` | 设备清单 manifest（App 连接时比对硬件兼容性）|

构建与架构细节见 [docs/DEVELOPMENT.md](../../docs/DEVELOPMENT.md)。
