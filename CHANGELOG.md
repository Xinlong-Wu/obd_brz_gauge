# 更新日志

[English](CHANGELOG.en.md) | 简体中文

最新的在最上面。只记录行为变化和烧录方式变化；纯重构和注释整理请查 git 历史。

---

## WS128 板型支持（Waveshare 1.28" 无触摸板）

新增第三块编译目标板型，现有 WS185/WS175 构建与行为不受影响：

- **新板型 `OBD_BOARD_WS_128_GC9A01`**：GC9A01 四线 SPI 240×240，
  ESP32-S3R2（封装内 2MB **Quad** PSRAM）。该板必须用
  `sdkconfig.defaults.ws128` 叠加层构建——公共 Octal PSRAM 默认会在
  它上面启动失败并循环重启（`octal_psram: PSRAM chip is not connected...`）
- **纯显示模式**：无触摸，跳过 LVGL 指针设备注册（此前无触摸板会在
  输入轮询时 assert）；V1 专属 RS485/ADS1115 初始化随 V2 硬件版本
  编译裁掉（该板 GPIO12 为 LCD_RST，与 RS485 RX 冲突）
- **虚拟 360 缩放输出**：UI 仍按 360×360 渲染，flush 阶段 3:2 最近邻
  降采样分块发屏（PSRAM 全帧影子 + DMA 分块 + 信号量同步），布局/
  主题/开机动画零改动
- **显示校准**：玻璃原生为左右镜像，初始化时 `esp_lcd_panel_mirror`
  仅开 X 轴校正；降采样分块发送必须**先等 DMA 完成信号量再重写缓冲**，
  先写后等会覆盖在途 DMA 数据，屏幕出现每 20 行一条的周期性横条错位
- **main 任务栈**：叠加层放宽到 8192（默认 3584 在该配置的 app_main
  初始化链路上溢出，启动即 `stack overflow in task main` 循环重启）
- 构建：`idf.py -B build_ws128 -DSDKCONFIG=sdkconfig.ws128
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ws128"
  set-target esp32s3 && build`（见 DEVELOPMENT.md 适配新开发板）

## 动态仪表页（用户自定义表盘）

主界面从静态轮播升级为**可自定义分页**：MENU → 仪表页(1..8) → ADD。

- **数据模型**：`ui_dashboard_cfg_t`（6 页 × 6 槽统一通道，METRIC/GEAR/
  G-FORCE 页型）按 NVS 追加红线并入配置；损坏自愈 + 老设备按现有
  TEMP/INFO/CHART/NEEDLE 映射自动迁移（外观对齐，开机停 TEMP）
- **运行时**：`ui_home_runtime` 渲染平铺——MENU（车型名 + BLE SCAN/
  SETTINGS/INFO-OTA 入口）、METRIC 槽位网格（ref 验证过的圆屏行模型
  [1]..[2,2,2]）、GEAR 大字挡位+RPM 弧、G-FORCE 点图、ADD 几何 "+"；
  左右滑切页、100ms 刷新（组件自读统一通道）
- **编辑**：长按仪表页 → EDIT/DELETE/BACK 蒙层；EDIT 进滚轮配置页
  （TYPE / SLOTS 1-6 / SLOT / CHANNEL 18 通道），改动即时持久化；
  ADD 追加默认 RPM 页（上限 8）
- **导航接管**：开机进 home；版本页（MENU → INFO/OTA）保留上滑 BLE/
  下滑设置/OTA 按钮/隐藏入口；加载带页面的运行时主题时仍先进主题页
- **设置页 BOOT PAGE 项移除**（被动态页取代）；静态轮播页
  （Temp/Info/Needle/Gear/Rpm/Speed 等）退役工作随后续清理提交完成
- 模拟器 `--home` 预览（现为 no-op，boot 天然进 home）；mock 与固件
  同款迁移/变更器；金图按新 UI 重生（tour 遍历 MENU/TEMP/INFO/GFORCE/
  CHART/ADD）

---

## 主题组件编排(schema 2.0)+ 打包器补齐

- **theme.bin schema 2.0**:清单新增 `components{}`(主题自定义组件,
  v1 原语组合、坐标相对组件矩形);页面布局可用 `instances[]` 编排
  「内置组件 + 主题组件」;固件 2.0 与 1.0 双支持,v1 主题行为不变,
  旧固件对 2.0 fail-closed 回退默认
- **内置组件库 `ui_component`**:value(名称+数值+单位)/ arc / bar /
  bignum / gforce 五件套,皮肤只走主题色角色、数据只走统一通道词汇表
  (`obd.*` ↔ disp_item),修正首写零值不渲染的脏检查 bug(配套回归断言)
- **pack_theme.py 补齐**(loader 早已支持、打包器缺的能力):多页
  `layouts/<page_id>.json`、任意命名资产(`.png` RGB565 / `.rgba.png`
  RGBA8888 / `.lv_font_bin` 字体)、`components.json` 嵌入、schema 自动
  升 2.0;图片打包泛化到任意尺寸(dial/ring 仍强制 360×360)
- **示例主题 `themes/example_v2_component/`**(value+arc+bar+gforce+
  theme:badge 编排)+ 截图回归新场景 `theme_v2_component`(先打包再
  预览,9/9 场景通过)
- 端到端验证:测试内构造 v2 blob 走完整加载→建页→刷新→渲染断言;
  模拟器预览视觉验收

---

## 板级抽象与 WS175 AMOLED 支持

- 新增板级抽象层 `bsp_obd_dsp/boards/`(移植自 Hokori23/obd_brz_gauge,
  实机验证过的实现):`board_api.h` 统一 init/显示上下文/亮度/共享 I2C/
  寄存器读写接口,`board_dispatch.c` 按 Kconfig **Display board** 静态分发;
  板文件以 `#if CONFIG_OBD_BOARD_*` 自守卫(组件 CMake 的 requirements 阶段
  取不到 CONFIG_ 变量,CMake 层不可分流)
- **新增 WS175**(微雪 ESP32-S3-Touch-AMOLED-1.75,466×466)构建目标:
  CO5300 QSPI 面板(CASET/RASET 出厂偏移补偿、首帧前保持黑屏)、CST9217
  触摸(失败降级无触摸并记入错误日志)、亮度命令 0x51、180° 安装由 LVGL
  `sw_rotate` 处理;新增组件依赖 esp_lcd_co5300 / esp_lcd_touch_cst9217 /
  esp_lcd_panel_io_additions;`sdkconfig.defaults.ws175` + 构建Overlay见
  docs/DEVELOPMENT.md(与 ref 的差异:旋转暂固定 180°,NVS 运行时旋转未引入)
- **WS185 行为零变化**:app_main 改经 board_* 调用(内部仍是原
  I2C_Init/EXIO_Init/LCD_Init 链),UI 屏幕符号统一走
  `board_display_compat.h` 门面;双板构建均通过 Docker 验证
- 未竟:QMI8658 IMU 驱动与 G-force 曲线(WS175 板上外设,下个专项随
  实机验证一起移植)

---

## NVS 诊断错误日志与初始化加固

- 新增**运行时错误日志**（64 条环形缓冲，约 5.9KB，持久化到 NVS
  `diag/errors`）：`nvs_error_log_record/recordf` 记录模块 tag、esp_err、
  开机秒数与消息；载入时自动修复损坏的版本号/游标；seq 跨重启单调。
  纯逻辑在 `nvs_error_log_logic.h`（`static inline`，单测 20+ 断言）；
  NVS 写失败会自记录（带递归保护）。读取入口（设置页/App/BLE）随
  M4 诊断页提供
- `nvs_storage_init()` 互斥量创建失败改为返回 `ESP_ERR_NO_MEM`，
  不再在首次上锁时随机崩溃（移植 ref 的加固；ref 的 flush 任务栈
  扩容不适用——本仓库统计不落盘、无后台 flush 任务）
- 模拟器 NVS mock 同步提供错误日志 API（内存环形缓冲，同一逻辑头）

---

## OBD 轮询档位（NORMAL / FAST / TURBO）

- 设置页新增 **OBD POLL** 滚轮（与 RACECHRONO 同排双列布局）：
  NORMAL 30ms（默认，兼容历史行为）/ FAST 15ms / TURBO 5ms，下一轮询周期
  即生效，无需重启
- 解析优先级不变：车型 override 的 `poll_gap_ms` > 车型 profile 的
  `poll_gap_ms` > 用户档位；已锁定间隔的车型（ZN/C6 CAN、MX-5 等）不受
  影响，档位只调"未锁定"车型的默认间隔
- NVS `nvs_user_cfg_t` 末尾追加 `obd_poll_mode`（老设备 grow 兼容，
  零值即 NORMAL）；单测 `tests/test_nvs_poll_mode.c`
- 移植自 Hokori23/obd_brz_gauge 的 turbo poll mode，间隔档位按本仓库
  30ms 基线重新定标

---

## ZC6 CAN 监听扩展：挡位直读 / G 力 / 胎压

- `ZN/C6 CAN` 车型的 ATMA 监听新增三个数据帧的解码（移植自
  Hokori23/obd_brz_gauge，三个行级解析器合并进现有
  `can_monitor_parse_line_fast` 管线，零新增轮询负载）：
  - **0x141 挡位直读**：N/1-6/R 直接写入数据缓存，优先于转速比值估算
    （未收到该帧的车型不受影响，自动回退比值估算）
  - **0x0D0 G 力**：纵向/横向加速度落入数据缓存（0.01g，显示页面
    属于后续里程碑）
  - **0x6E2 胎压**：四轮 0.1bar，单位（PSI/BAR/KPA）按"冷胎压力
    1.4~3.6 bar"启发式自动判定，判定结果重连前保持
- 数据缓存新增 `obd_data_{set,get}_gforce_x100` 与
  `obd_data_{set,get}_tpms_bar_x10`（快照结构未动，主题 ABI 不变）
- 解码逻辑为纯头文件 `zc6_monitor_decode.h`，配套单测
  `tests/test_zc6_monitor_decode.c`（28+ 断言）

---

## 测试与 CI 地基（不影响固件行为）

- **新增主机端单元测试 `tests/`**（CTest，零测试框架依赖）：复用
  `simulator/shims` 头遮蔽编译固件纯逻辑模块,首批覆盖数据缓存
  （哨兵/快照/RPM 覆盖层/挡位推算）、车型表（数量/越界钳制/数据自洽）、
  数据项系统（有效性判定/格式化/量程）、主题引擎（默认回退 + theme.bin v1
  加载 + 损坏容错）
- **新增截图回归 `tools/sim_regress.py`**（Pillow）：模拟器新增 `--seed`
  （固定假数据 PRNG）与 `--clock virtual`（帧锁定时钟）后截图**位级可复现**,
  8 个场景与 `tests/goldens/` 金图逐像素对比,headless 跑速约快 2 倍
- **新增 CI `.github/workflows/ci.yml`** 三 job:themes-check（主题生成物
  一致性）/ unit-sim（单测 + 截图回归,失败上传差图）/ firmware
  （Docker espressif/idf:v5.5.3 构建,产物上传 artifact）
- `theme_engine_test()` 由 void 改为 bool 返回（受保护页可主题化时报告失败）,
  开机自检接线不变

---

## PC 模拟器与 Docker 构建（不影响固件行为）

- **新增 `simulator/` PC 端模拟器**：固件 UI 源码（`export_path/`、`theme_engine/`、
  数据缓存、开机动画播放器）**零改动**编进 SDL2 窗口（LVGL 8.4 多显示器）。
  功能：假数据行驶场景、BLE 扫描→连接与从表配对全流程模拟、右侧数据调节面板
  （11 通道滑条 / 挡位旋轮 / 引擎开关，拖动即接管通道）、开机动画与运行时主题
  （theme.bin）加载、无头截图验收（`--frames/--screenshot/--tour/--tap`）。
  ESP-IDF 依赖由 `simulator/shims/` 遮蔽补齐，固件源码无一行改动；
  详见 [simulator/README.md](simulator/README.md)
- **新增 `tools/docker-build.sh`**：espressif/idf:v5.5.3 容器构建的薄封装，
  无本地 IDF 环境（如 macOS 容器无法烧录）时用容器编译、宿主机烧录

---

## 移除预编译固件渠道

- **仓库不再托管预编译固件**：删除 `firmware/release/`（含 `latest.json`）和游离的
  `firmware/obd_brz_gauge.bin`；烧录一律用本地 `build/` 产物，README 与
  [FLASH.md](docs/FLASH.md) 的烧录指引已同步改写。配套 App 的 OTA manifest 托管在
  自建服务器上（不在本仓库），App OTA 不受影响
- `tools/gen_release.py` 删除，`tools/release.sh` 简化为「commit → build → push」
- `build/` 停止 git 跟踪（此前整个构建目录曾被强加入库）；该目录本就在 `.gitignore` 中
- 配套 App 的 APK（`android_app/`）也不再随仓库分发，文档改为注明「不随本仓库分发」

---

## 文档体系重建（中英双语）

本分支相对 main 的全部改动汇总为一次文档重建，不涉及任何固件行为：

- **重构**：根 README 只做入口（定位、快速开始、索引、3D 模型、许可证），细节按受众拆成七份主题文档——`USER_GUIDE`（使用）、`FLASH`（烧录与升级，用户向）、`APP_PROTOCOL`（App 对接协议，开发者向）、`VEHICLES`（车型适配）、`THEMES`（主题，四份旧文档合并）、`TROUBLESHOOTING`（排障，三份旧文档合并）、`DEVELOPMENT`（开发指南，含发布流程）；`firmware/`、`themes/`、`theme_store/` 三个目录 README 精简为指引页。删除 12 份过时文档，包括描述已不存在的双分支世界的 `docs/BRANCH_COMPARISON.md`
- **修正危险的过时信息**：旧文档的部分表格仍把 bootmedia 烧到 `0x620000`——该地址现在是 theme_0 主题分区，照旧文档操作会毁掉主题分区；统一为 bootmedia `0xA20000` / 主题 `0x620000`，以 `partitions.csv` 为准。同时修正车型数量（12 → **17**）、`model/Subaru/brz_zc6/` 路径、"app 分区 4MB"（实为 3MB）等过期信息
- **双语**：11 份文档各配 1:1 镜像英文版（`X.md` ↔ `X.en.md`），标题下带语言切换行，英文版内部互链走 `.en.md`；中文为事实源，双语维护规则见 `DEVELOPMENT.md`
- **文档性代码注释修正**：`partitions.csv` bootmedia 容量注释 7.875MB → 5.875MB；`ads1115_oil_pressure.h` 头注释由"直连 ESP32 ADC"改为 ADS1115 I2C ADC；`make_boot_block.py` docstring 补 v2 格式；`one_shot.py` / `analyze_proble.py` 修正错误文件名 `analyze_probe.py`

## 主题引擎修复与 BMW 挡位直读

- BMW F/G 车型通过 EGS 扩展寻址 DID `DA2E` 直读当前挡位
  （请求头 `ATSH6F1`、接收过滤 `ATCRA618`），替代传动比估算
- 主题数据快照加入进气温度（IAT），修复 OBD 挡位页在无数据时显示 "--" 的问题
- 主题引擎回滚到 build 118（条件规则引擎引入的回归）
- 修正 CST816 触摸 I2C 引脚，触摸走独立总线

## 三连表联动转速报警同步

- 主表把 `rpm_warn_linked_en` 配置随广播同步给从表，三块表无需逐块设置
- 联动模式下三表按表位依次亮起、到阈值全体闪烁

## 新增车型（内置 17 个）

- 新增 `Supra A90`（B58，OBD 油压 DID `4436` 替代 ADS1115）、`BMW E`
  （N55 油温 `4402`/`5822` 与油压 `586F` 走 `6F1` 头）、`MINI R55`、`jeep`、
  `Honda Integra`（29 位功能寻址 `18DB33F1`，CVT 禁用挡位估算）
- 完整能力表见 `docs/VEHICLES.md`

## V2/V3 硬件变体（tai_ji_xiao_pai 24/25）

- 新增编译期硬件版本选择（menuconfig → OBD DSP Configuration）：
  V1 微雪板（默认）/ V2 新板 A（ST77916 v1 初始化序列）/ V3 新板 B（v2 序列），
  V2/V3 为直连 GPIO、无 TCA9554 / ADS1115

## 主题分区系统（theme_0）

- 新增 4MB `theme_0` 分区（`0x620000`）承载运行时主题：manifest + layout.json +
  360×360 表盘/表环素材，开机由 `theme_engine` 加载，损坏自动回退内置默认主题
- **bootmedia 从 `0x620000` 移到 `0xA20000`，容量 9.875MB → 5.875MB**（实际用量约 312KB）
- WiFi OTA 新增 `/ota/theme`、`/ota/theme/prepare`、`/ota/theme/erase` 端点；
  BLE 侧主题传输暂未实现
- 主题声明的页面替换内置表盘页进入轮播环（省 40–60 KB RAM）；
  `logo` / `intro` / `boot_video` 为受保护页，永不主题化
- ⚠️ 老分区表设备升级必须**一次性 USB 全量重刷**，OTA 无法改写分区表

## 设备端 OTA：BLE 服务 + WiFi 传输

- 新增 **OTA 模式页**（**版本页 OTA 按钮进入**）：发布 OTA BLE 服务（`0x1FFB`）并启动
  WiFi SoftAP（`OBD-Gauge-OTA-XXXX`，密码 `obd2024`），HTTP 端点接收
  SHA256 校验的固件与开机动画
- 固件写入备用 OTA 槽，启动 15 秒自检通过才标记有效，早期崩溃由 bootloader 回滚
- 开机动画更新事务化：`boot_block.txt.new`/`bin.new` 暂存后原子提交，中断可恢复；
  传输期间 RS485 与 ESP-NOW 暂停
- 版本页显示固件 build tag（分支-提交数-短哈希，编译时注入）

## RaceChrono 开关与开机动画模式简化

- 设置页新增 **RACECHRONO** 开关：关闭时进入最小 BLE 模式（仅 Info + OTA 服务，
  不广播），开启恢复完整 RaceChrono + 配对 + OTA 服务
- 开机动画模式简化为 **OFF / RACE / VIDEO**；VIDEO 播放手机 App 刷入的自制动画
  （替代旧的 REI/SHINJI/ASUKA 三个槽位，旧值自动迁移到 VIDEO）

## OBD 数据链路修复

- ELM327 客户端改为**单线程轮询**，不再混跑 OBD/CAN 并行请求
- 刹车温度 / 油压报警节流为 30 秒一次
- 数据中断自愈：重初始化 + 自动重连，上车通电无需手动重连

## OTA 双槽布局与设备清单

- 分区表改为 `ota_0` + `ota_1`（各 3MB）+ `bootmedia`，支持升级失败回滚
- 只读 BLE 设备清单服务（`0x1FFA`）暴露硬件与构建信息，App 刷写前做兼容校验；
  清单精简为 App 实际使用的字段，确保 512 字节内不截断

## 主题数据化（编译期 TOML 主题）

- 主题从 C 代码改为**数据声明**：`themes/` 下的 `theme.toml` + 素材 PNG，
  `tools/gen_themes.py` 在 CMake configure 阶段生成 `ui_theme_generated.c`
- 8 个装饰色角色 + 3 类可选素材（表框/指针/表盘），总预算 1536 KB
- `registry.txt` 槽位表只能追加，防止 OTA 后在用设备被静默换肤
- 语义色（报警红等）全局固定，任何主题不可改
