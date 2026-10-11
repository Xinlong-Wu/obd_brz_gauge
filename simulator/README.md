# PC 模拟器（LVGL SDL）

[English](README.en.md) | 简体中文

把固件 JDM UI 原样跑在电脑上：`ui/JDM/`、`obd_data_cache`、`vehicle_profiles`
**零改动**编译进 SDL2 窗口，鼠标当触摸，假数据当车。改 UI 不再需要编译固件 +
烧板，秒级看效果；也支持无头模式跑截图做自动化验收。

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
./simulator/build/obd_gauge_sim            # splash → JDM 页面 → 假数据行驶
```

- LVGL 用的是 `managed_components/lvgl__lvgl`（与固件同版本 9.6，由
  `dependencies.lock` 锁定）；目录不存在时自动 FetchContent v9.6.0。
- 鼠标左键 = 触摸，左右拖动 = 手势翻页（与真机轮播手势一致）。

## 常用玩法

```bash
# 默认流程：splash(~1.2s) → JDM 轮播首页，假数据开跑
./simulator/build/obd_gauge_sim

# 隐藏右侧数据面板（纯 360×360 窗口；CI 截图回归用此模式）
./simulator/build/obd_gauge_sim --no-panel

# 非 360 渲染分辨率（模拟 WS128 @240 / WS175 @466）
./simulator/build/obd_gauge_sim --ui-res 240
./simulator/build/obd_gauge_sim --ui-res 466
```

## 数据调节面板

窗口右侧内嵌一个 240×360 的 LVGL 面板（第二显示器，橙色 dark 主题），表盘渲染机制不变：

- **引擎开关**（右上，默认 ON）：ON = 假数据场景驱动所有通道，滑条位置实时跟随；OFF = 全部通道手动，滑条值直接写数据缓存
- **11 个通道滑条**：RPM / SPD / CLT / OIL / IAT / OILP / BST / BAT / AFR / TPS / BRAKE——拖动某一路即接管该通道（引擎让位，直到按 AUTO 或重开引擎）；引擎值为无效态时（如冷车油温）该行显示 `--` 且不写缓存
- **GEAR 旋轮**：AUTO 跟随场景，选 N/1-8 即手动指定
- **连接区**：BLE 状态按钮（JDM 页面当前直接读数据缓存，连接态仅作演示）

CLT 滑条拉到 -40 即数据无效（无效哨兵）。`--no-panel` 隐藏面板，恢复纯 360×360 窗口。

## CLI 参数

| 参数 | 说明 | 默认 |
|------|------|------|
| `--scale N` | 窗口缩放（360×N 像素） | 2 |
| `--ui-res N` | 仪表渲染分辨率（模拟 `CONFIG_OBD_UI_RENDER_RES`） | 360 |
| `--scenario NAME` | 假数据场景：`drive`（点火→热车→加减速循环）/ `idle`（原地怠速） | drive |
| `--profile N` | 车型配置序号 | 0（OBD2 Generic）|
| `--role ROLE` | `master` / `slave` / `standalone`（仅影响 NVS mock） | standalone |
| `--bound` | NVS 预存适配器 SIM-ELM327 | 关 |
| `--disconnected` | 配合 `--bound`：显示未连接态 | 关 |
| `--no-panel` | 隐藏右侧数据调节面板 | 关 |
| `--bootmedia DIR` | 开机动画文件目录 | `<repo>/bootmedia/slot_a` |
| `--tap X,Y,FRAME` | 脚本化点击：第 FRAME 帧在屏幕坐标 (X,Y) 注入一次触摸 | 关 |
| `--frames N` | 跑 N 帧后退出（无头模式用） | 0（窗口关闭才退）|
| `--screenshot FILE` | 退出前保存最后一帧 BMP | 无 |
| `--tour N` | 自动左右滑 N 次，每次截图到 `--shots-dir` | 0 |
| `--shots-dir DIR` | 巡览截图输出目录 | `sim_shots` |
| `--seed N` | 固定假数据 PRNG 种子（确定性截图，0 = 墙钟随机） | 0 |
| `--clock MODE` | `real`（默认，墙钟）/ `virtual`（帧锁定时钟：一虚拟帧 = 5ms，截图位级可复现） | real |

## 无头验证（CI / 自动截图）

```bash
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --clock virtual --frames 500 --screenshot /tmp/gauge.bmp
```

正式的截图回归用 `python3 tools/sim_regress.py`：固定 `--seed` + `--clock virtual`
跑一组场景，与 `tests/goldens/` 金图逐像素对比（见 [tools/README.md](../tools/README.md)）。
有意改了 UI 后用 `--update-goldens` 刷新金图并入库。

## 架构

```
固件源码（零改动）                    shim 层（全部在 simulator/ 内）
├── ui/JDM/**/*.c                    ├── include/: esp_log/esp_timer/freertos/*
├── app_obd_dsp/obd_data_cache.c     │   等遮蔽头（-I 顺序在最前，同名头优先生效）
├── app_obd_dsp/vehicle_profiles.c   └── src/:
└──（main/app_main.c 不编译）            ├── nvs_storage_mock.c   内存版用户配置
                                        ├── ota_boot_stubs.c     开机动画文件后端
                                        ├── bsp_stubs.c          BLE/ESP-NOW 假应答
        src/main.c: SDL2 窗口 + 双显示器 flush + 鼠标路由 + 启动序列复刻 app_main
        src/fake_data.c: 纯场景计算器（fake_data_compute），不碰缓存
        src/control_panel.c: 面板 tick——按引擎/手动把通道值写入 obd_data_cache
                            （--no-panel 时由 main.c 的 engine_apply_tick 代写）
```

要点：

- **渲染**：LVGL 软件渲染（native RGB565）→ `flush_cb` 脏区直推
  `SDL_UpdateTexture` → 窗口。
- **数据面**：走真实 `obd_data_cache`，假数据通过公共 setter 注入；里程统计
  任务（`vMileageDataStatisticTask`）也真实运行（esp_timer 协作式模拟）。
- **事件**：`app_event_recv` 恒空（模拟器单线程，无 ESP-NOW/BLE 生产者）。
- **NVS 不持久化**，重启即回默认值。

## 已知限制

- 挡位由假数据显式给定，不按车型传动比估算（`--profile` 影响的是车型名，
  以及依赖 profile 的解码逻辑）
- RaceChrono 输出、真实 OBD/BLE 通信、OTA 上传不可用
- 帧率/触摸手感与真机（QSPI 80MHz + CST816/9217）无关，视觉验收以真机为准
