# 工具脚本

[English](README.en.md) | 简体中文

主机侧开发/调试工具，不参与固件编译。构建期例外有两个：`gen_assets.py` 在
CMake 配置阶段、`gen_bootmedia.py` 在固件编译阶段被 `main/CMakeLists.txt`
自动调用。逐工具速查表见
[docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md#工具脚本)。

## 构建 / 发布 / 测试

| 脚本 | 用途 |
|------|------|
| `gen_assets.py` | 图片素材代码生成：`assets_src/images/*.png` → `ui/JDM/assets/images{,_r240,_r466}/*.c`（多分辨率）。CMake 配置期自动跑；`--check` 校验入库生成物是否过期 |
| `gen_jdm_fonts.py` | JDM 字体内嵌：`ui-prototype/fonts/*.ttf` → `ui/JDM/assets/fonts/jdm_*.c/.h`（TinyTTF 运行时栅格化）。`--check` 校验新鲜度（零第三方依赖）|
| `gen_bootmedia.py` | 编译期出厂开机动画镜像：`bootmedia/slot_a/` 的 manifest+帧流 → raw 布局 `bootmedia.bin`（manifest 补齐 4KB 槽后拼接帧数据，缺 `binary_size` 自动注入）。manifest 缺必需键直接构建失败，防止出厂静默无动画 |
| `release.sh` | 一键发版：commit（**必须先提交**，build tag 的 count 取 git 提交数）→ `idf.py build` → push |
| `sim_regress.py` | 模拟器截图回归（Pillow）：固定 seed + 虚拟时钟无头跑场景，逐像素对比 `tests/goldens/` 金图。`--update-goldens` 在有意改 UI 后刷新金图。CI 的 unit-sim job 调用 |

## 离线资源转换（产物入库，改素材时手动跑）

| 脚本 | 依赖 | 用途 |
|------|------|------|
| `make_boot_block.py` | ffmpeg + Pillow | 视频 → 开机动画 `bootmedia/slot_a/`（`boot_block.bin/txt` + `boot.pcm`）|

## 调试 / PID 挖掘（无需实车）

| 脚本 | 用途 |
|------|------|
| `fake_elm327.py` | 假装 WiFi ELM327 的 TCP 服务器（`:35000`）：记录 App 发的每条请求，未知 PID 一律返回肯定应答，诱其问完全套私有 PID。PID 挖掘用 `--probe access` 按访问计数注入，配对只需连续几个样本 |
| `one_shot.py` | 一键 PID 挖掘：起伪服务器 → 手机 Car Scanner 录一遍 → 自动跑分析出 `pids_full.csv` |
| `analyze_proble.py` | 把注入信号与 Car Scanner 录制按序配对，反推「哪个 PID 驱动哪个仪表、解码公式是什么」|
| `parse_carscanner_backup.py` | 解析 CarScanner App 备份目录，导出全车 PID 清单 CSV |

## 实车原始报文（需真实 ELM327 适配器）

| 脚本 | 依赖 | 用途 |
|------|------|------|
| `elm327_ble_dump.py` | bleak（`pip3 install bleak`）| 主机直连 BLE ELM327 读原始数据：按固件同款优先级识别 FFF0/18F0/FF12 服务，交互收发、`--cmd` 一次性脚本、`--loop` 轮询、`--all` 查支持位图后全量轮询写 CSV（复用 analyze_proble 标准表解码）、`--raw` 逐包十六进制 |
| `elm327_can_discover.py` | BLE: bleak; offline: stdlib | Generic CAN discovery: `scan` / `capture` → JSONL; `summary` / `analyze` → inventory / correlation candidates |

通用广播发现见 [CAN_DISCOVERY.md](../docs/CAN_DISCOVERY.md)：不限车型或预设信号，先盘点 ID，再用自由标签做重复状态对比。v1 仅 BLE；推荐虚拟环境内 `pip install bleak`，离线分析只用标准库。原始采集放仓库外、不提交或上传；输出不覆盖已有文件，父目录须先创建（如 `mkdir -p ~/can-captures`）。

新工具不发送车辆请求、不盲扫 DID；静默命令确认不等于克隆硬件电气层保证，不完整采集默认拒绝分析。既有 `elm327_ble_dump.py` 保持不变，主动 PID 查询与被动广播监听是两条不同路径，不能同时占用适配器。

## 一次性设备调试脚本（按需改串口）

| 脚本 | 用途 |
|------|------|
| `fix_nvs.py` | 把设备 NVS 角色重置为独立模式（修 WiFi OOM 崩溃）|
| `verify_bootmedia.py` | 读回 flash 里 bootmedia 分区前 512 字节，验证动画烧写 |

RS485/Modbus 硬件探针：`python_quick_rs485_check.py`、`python_test.py`（同在本目录）。
