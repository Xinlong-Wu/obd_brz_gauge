# 工具脚本

[English](README.en.md) | 简体中文

主机侧开发/调试工具，不参与固件编译。唯一的构建期例外是 `gen_themes.py`：
它被 `main/CMakeLists.txt` 在 CMake 配置阶段自动调用。逐工具速查表见
[docs/DEVELOPMENT.md](../docs/DEVELOPMENT.md#工具脚本)。

## 构建 / 发布

| 脚本 | 用途 |
|------|------|
| `gen_themes.py` | 编译期主题代码生成：`themes/registry.txt` + 各主题 `theme.toml` → `ui_theme_generated.c` + `theme_assets/`。CMake 配置期自动跑；`--check` 校验入库生成物是否过期（零第三方依赖）|
| `release.sh` | 一键发版：commit（**必须先提交**，build tag 的 count 取 git 提交数）→ `idf.py build` → push |

## 离线资源转换（产物入库，改素材时手动跑）

| 脚本 | 依赖 | 用途 |
|------|------|------|
| `make_boot_block.py` | ffmpeg + Pillow | 视频 → 开机动画 `bootmedia/slot_a/`（`boot_block.bin/txt` + `boot.pcm`）|
| `convert_rpm_flash.py` | Pillow | 3 张 PNG → RPM 报警闪烁图 `main/export_path/images/imgRpmFlash{1,2,3}.c` |
| `theme_packer/pack_theme.py` | Pillow | 运行时主题目录 → 4MB `theme.bin` 分区镜像（App 经 OTA 推给设备）|
| `gen_theme_store.py` | — | 扫描 `theme_store/<id>/` 重新生成 `catalog.json`（主题商店索引，App 拉取）|

## 调试 / PID 挖掘（无需实车）

| 脚本 | 用途 |
|------|------|
| `fake_elm327.py` | 假装 WiFi ELM327 的 TCP 服务器（`:35000`）：记录 App 发的每条请求，未知 PID 一律返回肯定应答，诱其问完全套私有 PID |
| `one_shot.py` | 一键 PID 挖掘：起伪服务器 → 手机 Car Scanner 录一遍 → 自动跑分析出 `pids_full.csv` |
| `analyze_proble.py` | 把注入信号与 Car Scanner 录制按序配对，反推「哪个 PID 驱动哪个仪表、解码公式是什么」|
| `parse_carscanner_backup.py` | 解析 CarScanner App 备份目录，导出全车 PID 清单 CSV |

## 一次性设备调试脚本（按需改串口）

| 脚本 | 用途 |
|------|------|
| `fix_nvs.py` | 把设备 NVS 角色重置为独立模式（修 WiFi OOM 崩溃）|
| `verify_bootmedia.py` | 读回 flash 里 bootmedia 分区前 512 字节，验证动画烧写 |

RS485/Modbus 硬件探针：`python_quick_rs485_check.py`、`python_test.py`（同在本目录）。
