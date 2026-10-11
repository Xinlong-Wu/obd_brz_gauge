# 烧录与升级

[English](FLASH.en.md) | 简体中文

## 分区表

16MB Flash，权威来源 [partitions.csv](../partitions.csv)：

| 分区 | 偏移 | 大小 | 用途 |
|------|------|------|------|
| nvs | 0x9000 | 24 KB | 用户设置 |
| otadata | 0xF000 | 8 KB | OTA 槽位选择 |
| phy_init | 0x11000 | 4 KB | 射频校准 |
| ota_0 | 0x20000 | 3 MB | 固件槽 A |
| ota_1 | 0x320000 | 3 MB | 固件槽 B |
| bootmedia | 0x620000 | 5.875 MB | 开机动画 |

> 旧分区表中的 theme_0（0x620000，4 MB）已随主题架构移除，bootmedia 前移到
> 其原地址。**已部署设备 OTA 到新固件后开机动画数据错位，需 USB 重新烧录
> bootmedia.bin**（OTA 无法改写分区表本身）。

## USB 烧录

固件自行编译（见 [README](../README.md) 快速开始），`idf.py build` 之后用
[esptool.py](https://github.com/espressif/esptool)（`pip install esptool`）烧录 `build/` 下的产物：

```bash
esptool.py --chip esp32s3 -p PORT -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0      build/bootloader/bootloader.bin \
  0x8000   build/partition_table/partition-table.bin \
  0xf000   build/ota_data_initial.bin \
  0x20000  build/obd_brz_gauge.bin \
  0x620000 build/bootmedia.bin
```

- `bootmedia.bin`（开机动画）可选，不烧只是没有开机动画。镜像为
  raw 布局（构建期 `tools/gen_bootmedia.py` 自动生成：偏移 0 是 manifest
  文本、0x1000 起是帧数据），不是文件系统镜像

## App OTA 升级（双槽回滚）

配套手机 App（不随本仓库分发）可以推送固件和开机动画。JDM UI 重写后
**设备端 OTA 入口页已移除**（HTTP 服务端 `ota_wifi_server` 保留，当前无
UI 触达路径，待后续按 JDM 设计语言补回入口页）。固件升级流程：

1. App 取 `latest.json`，BLE 读设备清单比对硬件兼容性，不匹配会拒绝刷写
2. 新固件写入**当前未运行的 OTA 槽**（`esp_ota_ops`）
3. 设备重启进新槽，**开机 15 秒自检**通过才标记有效；
   新固件在早期启动崩溃 → bootloader 自动回滚到旧槽

开机动画经 WiFi OTA 更新：请求体是 `[manifest][bin]` 拼接流（头部与端点见
[APP_PROTOCOL.md](APP_PROTOCOL.md)）。设备先擦除并写入 0x1000 起的帧数据区，
**manifest 最后落盘**——传输/掉电中断只会让 manifest 保持无效（开机跳过
动画），不会播放到损坏的半成品。传输期间 RS485 采集暂停让出 CPU。

BLE / WiFi 传输的完整协议（服务 UUID、HTTP 端点、请求头）见
[APP_PROTOCOL.md](APP_PROTOCOL.md)。
