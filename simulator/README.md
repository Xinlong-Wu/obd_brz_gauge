# PC 模拟器（LVGL SDL）

[English](README.en.md) | 简体中文

把固件 UI 原样跑在电脑上：`main/export_path/`、`theme_engine/`、`obd_data_cache`、
`vehicle_profiles`、`boot_block_player` **零改动**编译进 SDL2 窗口，鼠标当触摸，
假数据当车。改 UI 不再需要编译固件 + 烧板，秒级看效果；也支持无头模式跑截图
做自动化验收。

**固件源码一行不改**——所有 ESP-IDF 依赖由 `shims/` 层补齐（include 路径遮蔽 +
桩实现）。最终视觉与触摸手感仍以真机为准。

## 快速开始

```bash
# macOS
brew install cmake sdl2
# Linux (Debian/Ubuntu)
sudo apt install cmake libsdl2-dev

cmake -S simulator -B simulator/build
cmake --build simulator/build -j
./simulator/build/obd_gauge_sim            # 开机动画 → 默认页 → 假数据行驶
```

- LVGL 用的是 `managed_components/lvgl__lvgl`（与固件同版本 8.4.0，由
  `dependencies.lock` 锁定）；目录不存在时自动 FetchContent v8.4.0。
- 首次配置需联网拉 cJSON v1.7.18（theme manifest 解析用；系统装了
  cJSON 则直接用系统的）。
- 鼠标左键 = 触摸，左右拖动 = 手势翻页（与真机手势轮播一致）。

## 常用玩法

```bash
# 跳过开机动画，直接看表盘
./simulator/build/obd_gauge_sim --no-boot

# 换编译期主题（themes/registry.txt 槽位号）
./simulator/build/obd_gauge_sim --theme-slot 1      # amber

# 加载运行时主题（theme.bin 当 theme_0 分区）
./simulator/build/obd_gauge_sim --theme theme_store/boost_oil_example/theme.bin

# 模拟从表（不连 ELM327，等主表数据——模拟器里永远等不到）
./simulator/build/obd_gauge_sim --role slave

# 模拟未连接适配器的界面
./simulator/build/obd_gauge_sim --disconnected
```

## CLI 参数

| 参数 | 说明 | 默认 |
|------|------|------|
| `--scale N` | 窗口缩放（360×N 像素） | 2 |
| `--scenario NAME` | 假数据场景：`drive`（点火→热车→加减速循环）/ `idle`（原地怠速） | drive |
| `--profile N` | 车型配置序号（同设置页 VEHICLE 顺序） | 0（OBD2 Generic）|
| `--theme-slot N` | 编译期主题槽位（`themes/registry.txt` 行号） | 0 |
| `--role ROLE` | `master` / `slave` / `standalone` | standalone |
| `--disconnected` | 模拟 ELM327 未连接 | 关 |
| `--no-boot` | 跳过开机动画（intro 模式 OFF） | 关 |
| `--theme FILE` | theme.bin 路径，作为 theme_0 伪分区 | 无（走内置主题回退）|
| `--bootmedia DIR` | 开机动画文件目录 | `<repo>/bootmedia/slot_a` |
| `--frames N` | 跑 N 帧后退出（无头模式用） | 0（窗口关闭才退）|
| `--screenshot FILE` | 退出前保存最后一帧 BMP | 无 |
| `--tour N` | 自动左右滑 N 次，每次截图到 `--shots-dir` | 0 |
| `--shots-dir DIR` | 巡览截图输出目录 | `sim_shots` |

## 无头验证（CI / 自动截图）

```bash
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --no-boot --frames 500 --screenshot /tmp/gauge.bmp

# 自动巡览一圈页面，逐页截图
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --tour 14 --shots-dir /tmp/sim_shots
```

## 架构

```
固件源码（零改动）                    shim 层（全部在 simulator/ 内）
├── export_path/ui*.c + screens/     ├── include/: esp_log/esp_timer/esp_partition/freertos/*
├── theme_engine/theme_loader.c      │   等遮蔽头（-I 顺序在最前，同名头优先生效）
├── app_obd_dsp/obd_data_cache.c     └── src/:
├── app_obd_dsp/vehicle_profiles.c       ├── nvs_storage_mock.c   内存版用户配置
├── app_obd_dsp/boot_block_player.c      ├── esp_partition_file.c theme.bin 伪分区
└──（main/app_main.c 不编译）            ├── ota_boot_stubs.c     开机动画文件后端
                                        ├── bsp_stubs.c          BLE/ESP-NOW 假应答
        src/main.c: SDL2 窗口 + flush 字节序换回 + 鼠标 indev + 启动序列复刻
        src/fake_data.c: lv_timer 驱动行驶场景，直接调 obd_data_set_*
```

要点：

- **渲染**：LVGL 软件渲染 → `flush_cb` 把 RGB565 脏区做字节序换回
  （`LV_COLOR_16_SWAP=1` 与固件素材一致）→ `SDL_UpdateTexture` → 窗口。
- **数据面**：走真实 `obd_data_cache`，假数据通过公共 setter 注入；里程统计
  任务（`vMileageDataStatisticTask`）也真实运行（esp_timer 协作式模拟）。
- **开机动画**：真 `boot_block_player.c` + 文件后端读仓库 `bootmedia/slot_a/`，
  manifest/bin 解码路径与固件完全一致。
- **运行时主题**：`theme_loader.c` 的 `esp_partition_*` 调用打到内存伪分区，
  `--theme` 提供的 theme.bin 即整个 theme_0 分区镜像（可用
  `tools/theme_packer/pack_theme.py` 打包自己的主题目录生成）。
- **事件**：`app_event_recv` 恒空（模拟器单线程，无 ESP-NOW/BLE 生产者）。

## 已知限制

- 挡位由假数据显式给定，不按车型传动比估算（`--profile` 影响的是设置页显示的
  车型名，以及依赖 profile 的解码逻辑）
- BLE 扫描页永远没有结果；配对、OTA、RaceChrono、三连表联动均不可用（桩返回
  失败/空）
- NVS 不持久化，重启即回默认值
- 帧率/触摸手感与真机（QSPI 80MHz + CST816）无关，视觉验收以真机为准
