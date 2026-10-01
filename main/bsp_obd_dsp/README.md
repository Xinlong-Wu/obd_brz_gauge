# bsp_obd_dsp — 板级支撑

[English](README.en.md) | 简体中文

硬件外设与通信链路驱动，目标板为微雪 ESP32-S3-Touch-LCD-1.85（`bsp_board.h`
是板级配置入口；V1/V2/V3 硬件版本经 menuconfig → OBD DSP Configuration 选择）。

| 模块 | 职责 |
|------|------|
| `elm327_ble_client.*` | ELM327 适配器 BLE 客户端；**单线程轮询状态机**，不要引入并行/交错请求 |
| `espnow_link.*`、`gauge_pair_ble_client.*`、`ble_adv_util.*` | 三连表联动：ESP-NOW 数据广播 + 蓝牙配对 |
| `lcd_driver/`（ST77916 + esp_lcd 组件）、`touch_driver/`、`i2c_driver/`、`exio/` | 屏幕、触摸、I2C 总线、IO 扩展 |
| `nvs_storage.*` | 用户配置持久化；`nvs_user_cfg_t` 新字段**只能追加到末尾**（老设备兼容）|
| `ads1115_oil_pressure.*` | ADS1115 I2C ADC 油压采集 |
| `rs485_brake_temp.*` | RS485 刹车温度读取 |
| `racechrono_ble_diy.*` | 作为 RaceChrono DIY 设备向手机输出 BLE 数据 |
