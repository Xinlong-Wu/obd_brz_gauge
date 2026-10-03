# PC Simulator (LVGL SDL)

[简体中文](README.md) | English

Run the firmware UI unmodified on your computer: `main/export_path/`,
`theme_engine/`, `obd_data_cache`, `vehicle_profiles` and `boot_block_player`
are compiled **without a single change** into an SDL2 window — mouse as touch,
fake data as the car. UI iterations no longer need a firmware build + flash;
results show up in seconds. A headless mode also drives screenshots for
automated acceptance.

**No firmware file is touched** — every ESP-IDF dependency is satisfied by the
`shims/` layer (include-path shadowing + stub implementations). Final visuals
and touch feel still need to be confirmed on real hardware.

## Quick start

```bash
# macOS
brew install cmake sdl2
# Linux (Debian/Ubuntu)
sudo apt install cmake libsdl2-dev

cmake -S simulator -B simulator/build
cmake --build simulator/build -j
./simulator/build/obd_gauge_sim            # boot video → default page → fake drive
```

- LVGL comes from `managed_components/lvgl__lvgl` (the exact firmware copy,
  8.4.0, pinned by `dependencies.lock`); if absent, v8.4.0 is fetched
  automatically.
- The first configure needs network access to pull cJSON v1.7.18 (used to
  parse the theme manifest); a system cJSON is used when available.
- Left mouse button = touch, drag left/right = gesture page turns (same
  carousel as the device).

## Common recipes

```bash
# Default flow: boot video → BLE scan page (OBDII / V-LINK / ELM327 v2.3 appear
#   over a few seconds) → click one → "Connecting..." 1.2s → gauges with fake data
./simulator/build/obd_gauge_sim

# Skip the scan page entirely (NVS pre-saved with SIM-ELM327)
./simulator/build/obd_gauge_sim --bound

# Gauge pages with the adapter shown as offline
./simulator/build/obd_gauge_sim --bound --disconnected

# Skip the boot video
./simulator/build/obd_gauge_sim --bound --no-boot

# Switch the compile-time theme (slot in themes/registry.txt)
./simulator/build/obd_gauge_sim --bound --theme-slot 1      # amber

# Load a runtime theme (theme.bin served as the theme_0 partition)
./simulator/build/obd_gauge_sim --bound --theme theme_store/boost_oil_example/theme.bin

# Slave mode: FIND MASTER lists SkyGauge-XXXX units; a tap pairs in 1s and data flows
./simulator/build/obd_gauge_sim --role slave
```

## CLI options

| Option | Meaning | Default |
|--------|---------|---------|
| `--scale N` | window scale (360×N pixels) | 2 |
| `--scenario NAME` | fake-data scenario: `drive` (ignition→warm-up→accel/cycle) / `idle` | drive |
| `--profile N` | vehicle profile index (same order as Settings → VEHICLE) | 0 (OBD2 Generic) |
| `--theme-slot N` | compile-time theme slot (`themes/registry.txt` line) | 0 |
| `--role ROLE` | `master` / `slave` / `standalone` | standalone |
| `--bound` | pre-saved adapter SIM-ELM327 in NVS (skips the scan page, boots to the gauges) | off |
| `--disconnected` | with `--bound`: show the adapter as not connected | off |
| `--no-boot` | skip the boot video (intro mode OFF) | off |
| `--theme FILE` | theme.bin path, served as the theme_0 pseudo partition | none (built-in fallback) |
| `--bootmedia DIR` | dir holding the boot animation files | `<repo>/bootmedia/slot_a` |
| `--tap X,Y,FRAME` | scripted tap: injects a touch at screen coords on the given frame | off |
| `--frames N` | run N frames then exit (headless mode) | 0 (until window closed) |
| `--screenshot FILE` | save the final frame as BMP | none |
| `--tour N` | inject N swipes, screenshotting after each | 0 |
| `--shots-dir DIR` | where tour screenshots go | `sim_shots` |

## Headless verification (CI / automated screenshots)

```bash
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --no-boot --frames 500 --screenshot /tmp/gauge.bmp

# walk the carousel automatically, one screenshot per page
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --tour 14 --shots-dir /tmp/sim_shots
```

## Architecture

```
Firmware sources (unmodified)          Shim layer (all inside simulator/)
├── export_path/ui*.c + screens/       ├── include/: esp_log/esp_timer/esp_partition/freertos/*
├── theme_engine/theme_loader.c        │   shadowing headers (first -I, so they win)
├── app_obd_dsp/obd_data_cache.c       └── src/:
├── app_obd_dsp/vehicle_profiles.c         ├── nvs_storage_mock.c   in-memory user config
├── app_obd_dsp/boot_block_player.c        ├── esp_partition_file.c theme.bin pseudo partition
└── (main/app_main.c is NOT compiled)      ├── ota_boot_stubs.c     boot-media file backend
                                            ├── bsp_stubs.c          BLE/ESP-NOW canned answers
        src/main.c: SDL2 window + flush byte-swap + mouse indev + app_main-style boot
        src/fake_data.c: lv_timer driving a scenario straight into obd_data_set_*
```

Key points:

- **Rendering**: LVGL software render → `flush_cb` byte-swaps the RGB565 dirty
  area (`LV_COLOR_16_SWAP=1`, matching the firmware image assets) →
  `SDL_UpdateTexture` → window.
- **Data plane**: the real `obd_data_cache`; fake data goes through the public
  setters. The mileage task (`vMileageDataStatisticTask`) really runs too (via
  a cooperative esp_timer emulation).
- **Boot video**: the real `boot_block_player.c` with a file backend reading
  the repo's `bootmedia/slot_a/`; manifest/bin decoding is the exact firmware
  path.
- **Runtime themes**: `theme_loader.c`'s `esp_partition_*` calls hit an
  in-memory pseudo partition; the `--theme` file is the whole theme_0 image
  (pack your own with `tools/theme_packer/pack_theme.py`).
- **Events**: `app_event_recv` is always empty (single-threaded simulator, no
  ESP-NOW/BLE producers).
- **BLE flow simulation**: unbound by default, so a plain run boots to the
  scan page; scanning discovers 3 fake adapters at 700ms intervals, a tap
  "connects" after 1.2s and jumps to the gauges; FIND MASTER lists 2 SkyGauge
  units and pairing succeeds after 1s, binding the master MAC (after which
  `slave_has_data` is true and slave pages show data). Scripted taps via
  `--tap X,Y,FRAME`; headless regression example:
  `SDL_VIDEODRIVER=dummy ... --tap 180,165,400 --frames 900 --screenshot out.bmp`

## Known limitations

- Gear is set explicitly by the fake data, not derived from per-vehicle gear
  ratio tables (`--profile` affects the vehicle name in Settings and any
  profile-driven decoding, not the shown gear)
- BLE scan/connect and slave pairing run on simulated data (fixed names, MACs,
  RSSI); OTA upload, RaceChrono output and real OBD communication are
  unavailable
- NVS does not persist; settings reset on relaunch
- Frame pacing and touch feel differ from the real board (QSPI 80MHz +
  CST816); visual acceptance is done on hardware
