# Tool scripts

English | [简体中文](README.md)

Host-side development/debug tooling; none of it takes part in compiling the
firmware. The build-time exceptions are `gen_assets.py` (invoked by
`main/CMakeLists.txt` at CMake configure time) and `gen_bootmedia.py`
(invoked during the firmware build). Quick
reference table: [docs/DEVELOPMENT.en.md](../docs/DEVELOPMENT.en.md#tool-scripts).

## Build / release / test

| Script | Purpose |
|--------|---------|
| `gen_assets.py` | Image asset codegen: `assets_src/images/*.png` → `ui/JDM/assets/images{,_r240,_r466}/*.c` (multi-resolution). Runs automatically at CMake configure; `--check` verifies the checked-in artifacts are fresh |
| `gen_jdm_fonts.py` | JDM font embedding: `ui-prototype/fonts/*.ttf` → `ui/JDM/assets/fonts/jdm_*.c/.h` (TinyTTF runtime rasterization). `--check` verifies freshness (zero third-party deps) |
| `gen_bootmedia.py` | Compile-time factory boot-animation image: manifest + frame stream from `bootmedia/slot_a/` → raw-layout `bootmedia.bin` (manifest zero-padded to the 4 KB slot, frames concatenated at 0x1000; injects `binary_size` when missing). A manifest missing required keys fails the build instead of shipping a silent no-animation image |
| `release.sh` | One-shot release: commit (**commit first** — the build tag's count is the git commit count) → `idf.py build` → push |
| `sim_regress.py` | Simulator screenshot regression (Pillow): headless scenarios with a fixed seed + virtual clock, compared pixel-by-pixel against the `tests/goldens/` goldens. `--update-goldens` refreshes them after an intentional UI change. Called by the CI unit-sim job |

## Offline asset conversion (outputs are committed; run manually after changing assets)

| Script | Deps | Purpose |
|--------|------|---------|
| `make_boot_block.py` | ffmpeg + Pillow | Video → boot animation in `bootmedia/slot_a/` (`boot_block.bin/txt` + `boot.pcm`) |

## Debugging / PID hunting (no car required)

| Script | Purpose |
|--------|---------|
| `fake_elm327.py` | Fake WiFi ELM327 TCP server (`:35000`): logs every request the app sends and answers unknown PIDs positively, luring it to poll its whole private PID set. For PID hunting use `--probe access`, which injects per access so a few consecutive samples pin a pairing |
| `one_shot.py` | One-shot PID mining: start the fake server → record a session with Car Scanner → run the analysis automatically into `pids_full.csv` |
| `analyze_proble.py` | Pairs injected signals with the Car Scanner recording to infer which PID drives which gauge and its decode formula |
| `parse_carscanner_backup.py` | Parses a CarScanner app backup directory into a full vehicle PID CSV |

## Raw traffic from a real car (needs a real ELM327 adapter)

| Script | Deps | Purpose |
|--------|------|---------|
| `elm327_ble_dump.py` | bleak (`pip3 install bleak`) | Host-side direct BLE ELM327 client for raw data: auto-detects FFF0/18F0/FF12 services with the same priority as the firmware; interactive shell, one-shot `--cmd` script, `--loop` polling, `--all` support-bitmap scan + full PID sweep into a CSV (decodes via the analyze_proble standard table), `--raw` per-packet hex dump |

## One-off device debug scripts (adjust the serial port as needed)

| Script | Purpose |
|--------|---------|
| `fix_nvs.py` | Resets the device NVS role to standalone mode (fixes WiFi OOM crashes) |
| `verify_bootmedia.py` | Reads back the first 512 bytes of the bootmedia partition to verify a flash |

RS485/Modbus hardware probes: `python_quick_rs485_check.py`, `python_test.py` (in this directory).
