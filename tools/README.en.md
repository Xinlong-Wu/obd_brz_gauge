# Tool scripts

English | [简体中文](README.md)

Host-side development/debug tooling; none of it takes part in compiling the
firmware. The only build-time exception is `gen_themes.py`: it is invoked
automatically by `main/CMakeLists.txt` at CMake configure time. Quick
reference table: [docs/DEVELOPMENT.en.md](../docs/DEVELOPMENT.en.md#tool-scripts).

## Build / release / test

| Script | Purpose |
|--------|---------|
| `gen_themes.py` | Compile-time theme codegen: `themes/registry.txt` + per-theme `theme.toml` → `ui_theme_generated.c` + `theme_assets/`. Runs automatically at CMake configure; `--check` verifies the checked-in artifacts are fresh (zero third-party deps) |
| `release.sh` | One-shot release: commit (**commit first** — the build tag's count is the git commit count) → `idf.py build` → push |
| `sim_regress.py` | Simulator screenshot regression (Pillow): headless scenarios with a fixed seed + virtual clock, compared pixel-by-pixel against the `tests/goldens/` goldens. `--update-goldens` refreshes them after an intentional UI change. Called by the CI unit-sim job |

## Offline asset conversion (outputs are committed; run manually after changing assets)

| Script | Deps | Purpose |
|--------|------|---------|
| `make_boot_block.py` | ffmpeg + Pillow | Video → boot animation in `bootmedia/slot_a/` (`boot_block.bin/txt` + `boot.pcm`) |
| `convert_rpm_flash.py` | Pillow | 3 PNGs → RPM alarm flash images `main/export_path/images/imgRpmFlash{1,2,3}.c` |
| `theme_packer/pack_theme.py` | Pillow | Runtime theme directory → 4 MB `theme.bin` partition image (pushed to the device via app OTA) |
| `gen_theme_store.py` | — | Rescan `theme_store/<id>/` and regenerate `catalog.json` (theme store index fetched by the app) |

## Debugging / PID hunting (no car required)

| Script | Purpose |
|--------|---------|
| `fake_elm327.py` | Fake WiFi ELM327 TCP server (`:35000`): logs every request the app sends and answers unknown PIDs positively, luring it to poll its whole private PID set |
| `one_shot.py` | One-shot PID mining: start the fake server → record a session with Car Scanner → run the analysis automatically into `pids_full.csv` |
| `analyze_proble.py` | Pairs injected signals with the Car Scanner recording to infer which PID drives which gauge and its decode formula |
| `parse_carscanner_backup.py` | Parses a CarScanner app backup directory into a full vehicle PID CSV |

## One-off device debug scripts (adjust the serial port as needed)

| Script | Purpose |
|--------|---------|
| `fix_nvs.py` | Resets the device NVS role to standalone mode (fixes WiFi OOM crashes) |
| `verify_bootmedia.py` | Reads back the first 512 bytes of the bootmedia partition to verify a flash |

RS485/Modbus hardware probes: `python_quick_rs485_check.py`, `python_test.py` (in this directory).
