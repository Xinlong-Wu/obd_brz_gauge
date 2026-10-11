# AGENTS.md

给在此仓库工作的 AI 编码代理的操作说明。本文件只放约束、红线和指路，
不重复文档内容；项目与架构细节见 [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md)。

**会话开始时若存在 `AGENTS.local.md`（个人偏好，不入库），先读它。**

## 这是什么项目

ESP32-S3 圆形车载仪表（微雪 1.85" 圆屏）：ESP-IDF 5.5.3 + LVGL 9，
BLE 连 ELM327 兼容适配器读车况，ESP-NOW 三连表联动。
UI 为 JDM 设计语言（`ui/JDM/`），设计稿是 `ui-prototype/` 的浏览器原型。

MX-5 NC/JDM 视觉原型位于 `ui-prototype/`；后续原型修改与预览使用仓库内文件，
不要回写会话临时副本（入口与导出方式见 `ui-prototype/README.md`）。
原型基础与共用控件分别改 `foundations.js` 和 `components.js`；规则见
`ui-prototype/DESIGN_SYSTEM.md`，生成的 `foundations.css` / `.json` 不手改。
固件 UI 改 `ui/JDM/`：设计 token 只在 `core/jdm_tokens.h`，色值经
`jdm_color()` 取，几何一律裹 `UIS()`（720 母版，见 `core/jdm_geo.h`）；
组件一文件一控件放 `widgets/`，页面编排放 `pages/`。

## 改代码必须同步更新文档

同一改动里代码和文档一起动，映射如下：

| 改了什么 | 必须同步 |
|----------|----------|
| 车型增删、PID / 协议 / 油温路径 | `docs/VEHICLES.md`（+.en.md）、README 车型清单与数量 |
| 分区表 `partitions.csv` | `docs/FLASH.md`（+.en.md）、README 快速开始——烧录地址必须一致，以 `partitions.csv` 为准 |
| OTA 端点 / BLE 服务 / 设备清单格式 | `docs/APP_PROTOCOL.md`（+.en.md）|
| 页面导航、设置项、开机动画行为 | `docs/USER_GUIDE.md`（+.en.md）|
| 设计 token / 颜色 / 字号阶梯 | `ui/JDM/core/jdm_tokens.h`、`core/jdm_fonts_logic.h` 与 `ui-prototype/foundations.js` 三处一致，并跑 `python3 tools/gen_jdm_fonts.py` |
| 任何行为或烧录方式变化 | `CHANGELOG.md` **和** `CHANGELOG.en.md` 各加一条 |

硬规则：

- 中文文档是**事实源**，英文版（`X.en.md`）在同一个提交里跟上；结构 1:1 镜像
- 命令、代码、JSON、日志输出、表格数据**永不翻译**
- 文档里的数字拿不准时读代码，事实源清单：分区地址 → `partitions.csv`；
  车型表 → `main/app_obd_dsp/vehicle_profiles.c`；HTTP 端点 →
  `main/app_obd_dsp/ota_wifi_server.c`；NVS 配置键 → `main/bsp_obd_dsp/nvs_storage.h`；
  数据通道词汇表 → `ui/JDM/core/jdm_data.h`（枚举值只能末尾追加）

## 代码红线

- NVS `nvs_user_cfg_t` **新字段只能追加到末尾**（老设备 NVS 兼容，见 `nvs_storage.c` 的 grow 逻辑）
- `ui/JDM/assets/`（字体/图片 C 数组）是 `tools/gen_jdm_fonts.py` / `gen_assets.py` 的生成物，永远不要手改
- disp_item_t 枚举值是持久化契约（NVS dashboard 槽项），只能末尾追加、不可改序
- ELM327 轮询是**单线程状态机**，不要引入并行/交错请求
- 发版前必须先 commit——build tag 的 count 取 git 提交数（`tools/release.sh` 流程）

## 构建与验证

- `idf.py set-target esp32s3 && idf.py build`；换开发板先在 menuconfig → OBD DSP Configuration 选硬件版本（V1/V2/V3）。无本地 IDF 环境用 `tools/docker-build.sh`（espressif/idf:v5.5.3 容器；macOS 容器无法烧录/监视）
- 素材/字体可独立快速验证：`python3 tools/gen_assets.py --check && python3 tools/gen_jdm_fonts.py --check`
- 主机端三层验证（无需开发板）：单测 `ctest --test-dir tests/build -R '^test_'`、截图回归 `python3 tools/sim_regress.py`（有意改 UI 先 `--update-goldens`）、固件 Docker 构建；新代码遵循 `docs/CODE_STYLE.md`（纯逻辑抽 `*_logic.h` 配单测）
- `sim_regress.py` **不自动构建**：改 UI 后先 `cmake --build simulator/build`（确认链接成功，别只 grep 输出）再跑回归——金图永远来自 `simulator/build/obd_gauge_sim` 现有二进制
- `build/` 下的二进制是构建产物，普通源码改动不要碰
- 没有实车/适配器时用 `tools/fake_elm327.py` 模拟 ELM327 调试

## 提交约定

- conventional commits（`feat:` `fix:` `docs:` `build:` `chore:`），与现有历史一致
- `CHANGELOG.md` / `CHANGELOG.en.md` 的提交由仓库所有者决定时机，不要自作主张带上

## 本文件的自更新协议

发现新的持久约束（比如踩了一次构建的坑、一条没写在这里的红线）时，
按三问归类后记录，**过时条目当场删除**，不保留:

1. 对**任何**在此仓库工作的人都成立、且能从代码/配置验证？ → 写进本文件（`AGENTS.md`）
2. 只对某个人的工作流、口味或本机环境成立（例："commit message 用中文"、"我用 eim 激活 IDF"、"我的串口是 /dev/cu.usbmodemXXX"）？ → 写进 `AGENTS.local.md`
3. 一次性 / 临时信息（某次调试的端口、临时绕过）？ → 哪里都不写

质量规则：新条目必须短、可验证（注明依据的文件或教训来源）；本文件总量
控制在 150 行内，超了先删旧的再加新的。
