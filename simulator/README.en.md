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
# Skip the boot video, go straight to the gauges
./simulator/build/obd_gauge_sim --no-boot

# Switch the compile-time theme (slot in themes/registry.txt)
./simulator/build/obd_gauge_sim --theme-slot 1      # amber

# Load a runtime theme (theme.bin served as the theme_0 partition)
./simulator/build/obd_gauge_sim --theme theme_store/boost_oil_example/theme.bin

# Act as a slave gauge (no ELM327, waits for master data that never comes)
./simulator/build/obd_gauge_sim --role slave

# UI as it looks with no adapter connected
./simulator/build/obd_gauge_sim --disconnected
```

## CLI options

| Option | Meaning | Default |
|--------|---------|---------|
| `--scale N` | window scale (360×N pixels) | 2 |
| `--scenario NAME` | fake-data scenario: `drive` (ignition→warm-up→accel/cycle) / `idle` | drive |
| `--profile N` | vehicle profile index (same order as Settings → VEHICLE) | 0 (OBD2 Generic) |
| `--theme-slot N` | compile-time theme slot (`themes/registry.txt` line) | 0 |
| `--role ROLE` | `master` / `slave` / `standalone` | standalone |
| `--disconnected` | simulate the bound adapter being powered off (still boots to the gauges, pages show the disconnected state) | off |
| `--unbound` | no saved adapter in NVS (boots to the BLE scan page, i.e. a freshly flashed device) | off |
| `--no-boot` | skip the boot video (intro mode OFF) | off |
| `--theme FILE` | theme.bin path, served as the theme_0 pseudo partition | none (built-in fallback) |
| `--bootmedia DIR` | dir holding the boot animation files | `<repo>/bootmedia/slot_a` |
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

## Known limitations

- Gear is set explicitly by the fake data, not derived from per-vehicle gear
  ratio tables (`--profile` affects the vehicle name in Settings and any
  profile-driven decoding, not the shown gear)
- The BLE scan page never finds devices; pairing, OTA, RaceChrono and
  triple-gauge sync are unavailable (stubs fail/return empty)
- NVS does not persist; settings reset on relaunch
- Frame pacing and touch feel differ from the real board (QSPI 80MHz +
  CST816); visual acceptance is done on hardware
