# theme_engine — 运行时主题加载器

[English](README.en.md) | 简体中文

从 theme_0 分区（4MB）加载运行时主题包。主题包由
`tools/theme_packer/pack_theme.py` 打包成 `theme.bin`，经手机 App OTA 推入；
`app_obd_dsp/theme_mount.c` 负责挂载分区。

它与编译期主题是**两套系统**：编译期主题（`themes/` + `tools/gen_themes.py`，
生成 `export_path/ui_theme_generated.c`）在固件里；本目录只管运行时可换装的
这套。全貌见 [docs/THEMES.md](../../docs/THEMES.md)。

| 文件 | 职责 |
|------|------|
| `theme_loader.c` | 校验并加载分区里的主题 manifest 与资产 |
| `theme_interface.h` | 与 UI 层之间的主题接口约定 |
| `theme_test.c` | 集成自检（从 `ui_init()` 调用，打印主题系统状态）|
