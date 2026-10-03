# 开发指南

[English](DEVELOPMENT.en.md) | 简体中文

改车型看 [VEHICLES.md](VEHICLES.md)，改主题看 [THEMES.md](THEMES.md)，
烧录升级看 [FLASH.md](FLASH.md)，App 对接协议看 [APP_PROTOCOL.md](APP_PROTOCOL.md)。

## 代码架构

```
main/
├── app_main.c            # 入口：硬件初始化、LVGL 锁、BLE/RS485/ESP-NOW 任务、OTA 自检
│
├── app_obd_dsp/          # 应用层（与板子无关的业务）
│   ├── obd_data_cache    # OBD 数据缓存（UI 与采集之间的唯一数据面）
│   ├── vehicle_profiles  # 17 车型配置 + 挡位区间计算（见 VEHICLES.md）
│   ├── vehicle_custom_config.h  # CAN 规则 / 油温公式 / UDS 头 override 表
│   ├── ota_update_ble    # BLE OTA 接收（固件 / 开机动画两种目标）
│   ├── ota_wifi_server   # WiFi SoftAP HTTP OTA 服务（全部 /ota/* 端点）
│   ├── device_identity   # BLE 设备清单 JSON（0x1FFA，硬件 + build_tag）
│   ├── boot_media_mount / boot_block_player  # bootmedia 分区挂载与开机动画播放
│   └── theme_mount       # theme_0 分区挂载辅助
│
├── bsp_obd_dsp/          # 板级支持包（换开发板主要改这里）
│   ├── elm327_ble_client # ELM327 BLE 客户端 + 单线程轮询状态机 + 协议自动检测
│   ├── espnow_link       # 三连表 ESP-NOW 链路（MASTER/SLAVE/STANDALONE）
│   ├── racechrono_ble_diy # RaceChrono DIY BLE GATT 服务 + 配对服务 + OTA 广播
│   ├── gauge_pair_ble_client # 从表 FIND MASTER 扫描（与 OBD 扫描共用页面）
│   ├── nvs_storage       # 用户配置与统计的 NVS 持久化（新字段只能追加到结构体末尾）
│   ├── ads1115_oil_pressure # ADS1115 I2C 油压 ADC（仅 V1 板；有 OBD 油压 DID 的车型不用它）
│   ├── rs485_brake_temp  # RS485 Modbus RTU 刹车温度采样任务（OTA 期间暂停）
│   ├── lcd_driver/       # ST77916 QSPI LCD（V2/V3 板 init 序列不同）
│   ├── touch_driver/     # CST816 触摸（I2C）
│   ├── exio/             # TCA9554 IO 扩展（仅 V1 板）
│   └── i2c_driver/       # I2C 主驱动
│
├── export_path/          # LVGL UI（SquareLine 导出 + 手写扩展）
│   ├── ui.c              # 页面创建顺序、手势导航（轮播环）、联动报警逻辑
│   ├── screens/          # 24 个页面：Logo/Main/Gear/ThemeGauge/Rpm/Speed/ODBProtocal/
│   │                     #   EasterEgg(版本页)/BLEScan/OTAMode/Temp/TempCustom/
│   │                     #   OilPressure(曲线页)/ChartConfig/ChartAlarm/Info/InfoCustom/
│   │                     #   Settings/OilWarn/RpmWarn/Needle/NeedleConfig/MultiGauge/Intro
│   ├── ui_ext.c          # 手写扩展（扫表动画、展厅模式、开机动画调度）——SquareLine 重导出时不会覆盖
│   ├── ui_disp_item.c    # 显示项枚举（TEMP/INFO 页可映射的数据源）
│   └── ui_theme*         # 编译期主题运行时（活动主题、NVS、访问器；生成文件见 THEMES.md）
│
└── theme_engine/         # 运行时主题引擎（theme_0 分区 → 页面/颜色/素材/数据绑定）
```

### 数据流

```
ELM327 BLE 适配器 ──BLE──→ elm327_ble_client（单线程轮询：标准 PID / 厂商油温 / 挡位 DID / ATMA*）
                                   │
                                   ▼
                            obd_data_cache ←── rs485_brake_temp（刹车温度）
                                   │      ←── ads1115_oil_pressure（油压，V1 板）
                 ┌─────────────────┼──────────────────┐
                 ▼                 ▼                  ▼
           LVGL 页面刷新     theme_update_data()   espnow_link（主表广播）
           （内置表盘页）     （运行时主题页面）           │
                                                          ▼
                                              从表 espnow_link 接收 → 直接渲染
```

*ATMA CAN 监听仅 `ZN/C6 CAN` 车型启用，其余全部走 OBD 轮询，避免适配器双路抢占。

**ELM327 轮询是单线程状态机**：初始化 → （可选）协议自动检测 → 循环轮询各通道。
改轮询逻辑时不要引入并行请求 —— 一个适配器同时只服务一个请求流。

## 编译配置

环境：ESP-IDF 5.5.3 + Python 工具链。组件依赖（`main/idf_component.yml`）：
`lvgl/lvgl`、`espressif/esp_lcd_touch`、`espressif/button`、`espressif/knob`，
首次构建自动拉到 `managed_components/`。

```bash
idf.py set-target esp32s3
idf.py menuconfig     # OBD DSP Configuration
idf.py build
idf.py -p PORT flash monitor
```

没有本地 IDF 环境时用 Docker——`tools/docker-build.sh` 是
`docker run --rm … espressif/idf:v5.5.3` 的薄封装，参数原样传给 `idf.py`：

```bash
tools/docker-build.sh set-target esp32s3   # 首次
tools/docker-build.sh build
tools/docker-build.sh menuconfig
```

产物落在宿主机 `build/`。macOS 的容器无法直通 USB，烧录/监视在宿主机跑
（`pip install esptool esp-idf-monitor`，烧录命令见 [FLASH.md](FLASH.md)）。

`menuconfig → OBD DSP Configuration`：

| 选项 | 值 | 说明 |
|------|----|------|
| 硬件版本 | **V1**（默认）| 微雪 ESP32-S3-Touch-LCD-1.85：ST77916 v2 init + TCA9554 + ADS1115 |
| | V2 | 新板 A：ST77916 **v1** init，直连 GPIO，无 TCA9554 / ADS1115 |
| | V3 | 新板 B：ST77916 v2 init，直连 GPIO，无 TCA9554 / ADS1115 |
| RS485 引脚 | TX 13 / RX 12 / DE-RE −1 | 刹车温度模块；TX 不要用 GPIO43/44（板载 USB 串口）|

构建系统在 CMake configure 阶段自动运行 `tools/gen_themes.py` 生成主题表，
并把 git 分支 / 提交数 / 短哈希注入 `OBD_GAUGE_BUILD_TAG`（设备清单和版本页显示的
build tag 就是它，所以发版前必须先 commit，见[发布流程](#发布流程)）。

## 适配新开发板

板级抽象在 `main/bsp_obd_dsp/boards/`(`board_api.h` 统一接口,`board_dispatch.c`
按 Kconfig `OBD DSP Configuration → Display board` 静态分发;新板=新增一个
`board_<id>.c` + spec 头 + Kconfig 选项,板文件用 `#if CONFIG_OBD_BOARD_<ID>`
自守卫——组件 CMake 的 requirements 阶段拿不到 CONFIG_ 变量,不能在 CMake 里分流)。
现有两块:

- **WS185**(默认):微雪 1.85" IPS,ST77916 QSPI + CST816 + TCA9554(V1/V2/V3 细分见 `OBD_HW_VERSION`)
- **WS175**:微雪 1.75" AMOLED 466×466,CO5300 QSPI + CST9217(共享 I2C 可挂
  QMI8658/ADS1115),180° 由 LVGL `sw_rotate` 处理;构建:

```bash
idf.py -B build_ws175 -DSDKCONFIG=sdkconfig.ws175 \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ws175" set-target esp32s3
idf.py -B build_ws175 -DSDKCONFIG=sdkconfig.ws175 build
```

UI 层包含屏幕符号请用 `boards/board_display_compat.h`(WS185 转发 ST77916.h,
WS175 提供同名宏/兼容壳),不要再直接 include ST77916.h。

1. `Kconfig.projbuild` 加硬件版本选项（若 LCD / 触摸 / IO 扩展不同）
2. 重点检查 `bsp_obd_dsp/`：`lcd_driver/`（初始化序列、QSPI 参数）、`touch_driver/`、
   `exio/`（有无 IO 扩展）、引脚定义
3. 屏幕分辨率 / 色深宏在 `ST77916.h`，UI 按 360×360 设计，换屏要动 `export_path/`
4. 与车相关的都不用动 —— 车型逻辑全在 `app_obd_dsp/`

## PC 模拟器（UI 预览）

UI 源码**零改动**跑在电脑上（SDL2 窗口，鼠标当触摸，假数据当车），改 UI 不用烧板：

```bash
brew install cmake sdl2          # Linux: sudo apt install cmake libsdl2-dev
cmake -S simulator -B simulator/build && cmake --build simulator/build -j
./simulator/build/obd_gauge_sim --no-boot
```

- ESP-IDF 依赖由 `simulator/shims/`（include 路径遮蔽 + 桩实现）补齐，固件源码不动；
  LVGL 直接用 `managed_components/` 里与固件同版本的那份（8.4.0）
- 支持编译期/运行时主题切换（`--theme-slot` / `--theme theme.bin`）、模拟未连接
  （`--disconnected`）、开关机动画（`--no-boot`）、假数据场景（`--scenario`）
- 无头截图验收：`SDL_VIDEODRIVER=dummy ... --frames 500 --screenshot x.bmp`，
  或 `--tour N` 自动巡览一圈逐页截图
- 里程统计、开机动画解码、主题 manifest 解析路径与固件完全一致；BLE/OTA/三连表
  为桩，不可用

参数表与架构说明见 [simulator/README.md](../simulator/README.md)。

## 测试与 CI

三层验证,全部在主机跑,不需要开发板:

```bash
# 1) 主机端单元测试(tests/,CTest;复用 simulator/shims 编译固件纯逻辑模块)
cmake -S tests -B tests/build && cmake --build tests/build -j
ctest --test-dir tests/build -R '^test_' --output-on-failure

# 2) 模拟器截图回归(金图对比;改 UI 后 --update-goldens 刷新并入库)
python3 tools/sim_regress.py

# 3) 主题生成物一致性(零依赖)
python3 tools/gen_themes.py --check
```

- 单测新增用例:在 `tests/` 加 `test_xxx.c` + `tests/CMakeLists.txt` 注册
  (`add_gauge_test`),断言宏见 `tests/test_util.h`;纯逻辑优先抽成
  `*_logic.h`(`static inline`,不依赖 LVGL/ESP-IDF)再测
- 截图回归依赖 `--seed` + `--clock virtual` 的确定性(同参数位级一致);
  金图在 `tests/goldens/`(PNG)
- CI(`.github/workflows/ci.yml`)三 job:themes-check / unit-sim(单测+
  截图回归,失败上传差图)/ firmware(Docker `espressif/idf:v5.5.3` 构建,
  产物上传 artifact)

## 工具脚本

| 脚本 | 用途 |
|------|------|
| `tools/gen_themes.py` | 编译期主题代码生成（CMake 自动跑；`--check` 供 CI）|
| `tools/theme_packer/pack_theme.py` | 运行时主题打包成 4MB theme.bin |
| `tools/gen_theme_store.py` | 重新生成主题商店 catalog.json |
| `tools/release.sh` | 一键发版（commit → build → push）|
| `tools/make_boot_block.py` | 视频编码成 boot_block.bin/txt（ffmpeg + Pillow；帧数超 65535 自动切 v2 格式）|
| `tools/convert_rpm_flash.py` | 3 张 PNG → RPM 报警闪烁图（imgRpmFlash1..3.c）|
| `tools/fake_elm327.py` | 伪 ELM327 TCP 服务器：记录 App 发的每条请求，未知 PID 一律返回肯定应答 |
| `tools/one_shot.py` | 一键 PID 挖掘：起伪服务器 → 手机录一遍 → 自动出 pids_full.csv |
| `tools/analyze_proble.py` | 把注入信号与 Car Scanner 录制对上号，反推 PID→仪表映射与解码公式 |
| `tools/parse_carscanner_backup.py` | 解析 CarScanner 备份目录，导出全车 PID 清单 CSV |
| `tools/python_quick_rs485_check.py` | RS485/Modbus 直连自检（排障用）|
| `tools/fix_nvs.py` | 把 NVS 角色重置为独立模式（修 WiFi OOM 崩溃）|

### PID 挖掘工作流（不上车找私有 PID）

想给新车型找私有 PID（油温、油压、挡位…）时，用伪适配器反向记录手机 App
（Car Scanner 自带各车 profile）的请求：

```bash
python3 tools/one_shot.py
# 1. 自动起 fake_elm327.py --probe prbs --tick 2（注入已知伪信号）
# 2. 手机 Car Scanner 连 WiFi ELM327（本机 IP:35000），选目标车型 profile，录一段导出 CSV
# 3. 粘贴 CSV 路径，自动停服务器并跑 analyze → pids_full.csv
```

原理：伪服务器对未知 PID 全部返回"肯定应答"（App 不会标记不支持，会把 profile
里的私有 PID 问全），每条注入信号对应 App 的一次真实请求，**按顺序配对即可反推
哪个 PID 驱动哪个仪表、解码公式是什么**。挖到的结果按
[VEHICLES.md](VEHICLES.md#新增车型) 加进车型配置。

## 发布流程

`tools/release.sh` 一键发版：激活 ESP-IDF 环境（eim）→ 提交源码（**必须先提交**，
`count` 取 git 提交数）→ `idf.py build` → 推送。仓库不再托管预编译固件；
烧录一律用本地 `build/` 产物（见 [FLASH.md](FLASH.md)）。

配套 App 的固件 OTA 从**自托管服务器**拉 manifest（不在本仓库）：

```text
<OTA 服务器>/releases/obd_brz_gauge/
  latest.json
  firmware/    obd_brz_gauge.bin · partition-table.bin · bootloader.bin · ota_data_initial.bin
  bootmedia/   bootmedia.bin
```

App 用 `latest.json` 里的 `firmware.count` 比对设备固件新旧；托管该目录的
服务器由仓库所有者自行维护。

## 提交约定

- 行为变化与烧录方式变化要记 [CHANGELOG.md](../CHANGELOG.md)
- NVS `nvs_user_cfg_t` / 主题结构体新字段**只能追加到末尾**（老设备 NVS 兼容）
- 主题相关提交别动 `ui_theme_generated.c` / `theme_assets/`（生成物）

### 双语文档维护规则

- 文档成对维护：`X.md`（中文，**事实源**）↔ `X.en.md`（英文译文）。
  事实改动先改中文，英文在**同一个提交**里跟上
- 结构 1:1 镜像（章节、表格、代码块一一对应），方便对照排查漂移；
  命令、代码、JSON、日志输出、表格数据一律不翻译
- 英文文档内部互链指向对应的 `.en.md` 版本；链到代码和分区文件的路径不变
- 每份成对文档标题下都有语言切换行（`[English](X.en.md) | 简体中文`）；
  新增文档时中英两份一起建
