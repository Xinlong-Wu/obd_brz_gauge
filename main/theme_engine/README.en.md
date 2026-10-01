# theme_engine — runtime theme loader

English | [简体中文](README.md)

Loads runtime theme packages from the theme_0 partition (4 MB). A package is
packed into `theme.bin` by `tools/theme_packer/pack_theme.py` and pushed over
app OTA; `app_obd_dsp/theme_mount.c` mounts the partition.

This is a **separate system** from compile-time themes (`themes/` +
`tools/gen_themes.py`, generating `export_path/ui_theme_generated.c`, which
live inside the firmware); this directory only handles the runtime-swappable
one. Overview: [docs/THEMES.en.md](../../docs/THEMES.en.md).

| File | Responsibility |
|------|----------------|
| `theme_loader.c` | Validates and loads the theme manifest and assets from the partition |
| `theme_interface.h` | Theme interface contract with the UI layer |
| `theme_test.c` | Integration self-check (called from `ui_init()`, logs theme system status) |
