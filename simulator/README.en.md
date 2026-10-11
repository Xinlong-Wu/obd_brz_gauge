# PC Simulator (LVGL SDL)

[简体中文](README.md) | English

Run the firmware JDM UI unmodified on your computer: `ui/JDM/`,
`obd_data_cache` and `vehicle_profiles` are compiled **without a single
change** into an SDL2 window — mouse as touch, fake data as the car. UI
iterations no longer need a firmware build + flash; results show up in
seconds. A headless mode also drives screenshots for automated acceptance.

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
./simulator/build/obd_gauge_sim            # splash → JDM pages → fake-data drive
```

- LVGL comes from `managed_components/lvgl__lvgl` (same 9.6 as the firmware,
  pinned by `dependencies.lock`); FetchContent v9.6.0 kicks in when missing.
- Left mouse button = touch; drag left/right = swipe between carousel pages
  (same gesture as the device).

## Common runs

```bash
# Default flow: splash (~1.2s) → JDM carousel home with fake data running
./simulator/build/obd_gauge_sim

# Hide the side data panel (plain 360x360 window; what CI screenshots use)
./simulator/build/obd_gauge_sim --no-panel

# Non-360 render resolutions (mimics WS128 @240 / WS175 @466)
./simulator/build/obd_gauge_sim --ui-res 240
./simulator/build/obd_gauge_sim --ui-res 466
```

## Data panel

The right side of the window hosts a 240×360 LVGL panel (second display,
orange dark theme); the gauge rendering path is untouched:

- **Engine switch** (top right, ON by default): ON = the fake-data scenario
  drives all channels and the sliders follow; OFF = all channels manual, slider
  values write the data cache directly
- **11 channel sliders**: RPM / SPD / CLT / OIL / IAT / OILP / BST / BAT / AFR /
  TPS / BRAKE — dragging one takes the channel over (the engine backs off until
  AUTO or the engine is re-enabled); when the engine value is invalid (e.g. a
  cold-oil temp) the row shows `--` and writes nothing
- **GEAR wheel**: AUTO follows the scenario; picking N/1-8 forces it
- **Connection area**: BLE state button (JDM pages currently read the data
  cache directly; connection state is demo-only)

Pull CLT to -40 to make the sample invalid (invalid sentinel).
`--no-panel` hides the panel and restores the plain 360×360 window.

## CLI options

| Option | Meaning | Default |
|--------|---------|---------|
| `--scale N` | Window scale (360×N pixels) | 2 |
| `--ui-res N` | Gauge render resolution (mimics `CONFIG_OBD_UI_RENDER_RES`) | 360 |
| `--scenario NAME` | Fake-data scenario: `drive` (ignition→warmup→accel loops) / `idle` | drive |
| `--profile N` | Vehicle profile index | 0 (OBD2 Generic) |
| `--role ROLE` | `master` / `slave` / `standalone` (NVS mock only) | standalone |
| `--bound` | Pre-saved SIM-ELM327 adapter in NVS | off |
| `--disconnected` | With `--bound`: show the adapter as not connected | off |
| `--no-panel` | Hide the side data panel | off |
| `--bootmedia DIR` | Boot animation file dir | `<repo>/bootmedia/slot_a` |
| `--tap X,Y,FRAME` | Scripted tap: inject a touch at (X,Y) on FRAME | off |
| `--frames N` | Exit after N frames (headless) | 0 (until window closed) |
| `--screenshot FILE` | Save the final frame as BMP | none |
| `--tour N` | Auto-swipe N times, screenshotting into `--shots-dir` | 0 |
| `--shots-dir DIR` | Tour screenshot output dir | `sim_shots` |
| `--seed N` | Pin the fake-data PRNG (deterministic screenshots, 0 = wall clock) | 0 |
| `--clock MODE` | `real` (default, wall clock) / `virtual` (frame-locked: one virtual frame = 5ms, bit-reproducible screenshots) | real |

## Headless verification (CI / automated screenshots)

```bash
SDL_VIDEODRIVER=dummy ./simulator/build/obd_gauge_sim \
    --clock virtual --frames 500 --screenshot /tmp/gauge.bmp
```

The official screenshot regression is `python3 tools/sim_regress.py`: scenarios
run with a fixed `--seed` + `--clock virtual` and compare pixel-by-pixel
against the `tests/goldens/` goldens (see [tools/README.en.md](../tools/README.en.md)).
After an intentional UI change, refresh the goldens with `--update-goldens`
and commit them.

## Architecture

```
Firmware sources (unmodified)         Shim layer (all inside simulator/)
├── ui/JDM/**/*.c                     ├── include/: esp_log/esp_timer/freertos/*
├── app_obd_dsp/obd_data_cache.c      │   shadowing headers (-I comes first)
├── app_obd_dsp/vehicle_profiles.c    └── src/:
└── (main/app_main.c is not built)        ├── nvs_storage_mock.c   in-memory user config
                                         ├── ota_boot_stubs.c     boot-media file backend
                                         ├── bsp_stubs.c          BLE/ESP-NOW fakes
        src/main.c: SDL2 window + dual-display flush + mouse routing + app_main-style bring-up
        src/fake_data.c: pure scenario calculator (fake_data_compute), never touches the cache
        src/control_panel.c: panel tick — writes channel values into obd_data_cache
                            (engine_apply_tick in main.c substitutes with --no-panel)
```

Notes:

- **Rendering**: LVGL software renderer (native RGB565) → `flush_cb` pushes
  dirty rects straight to `SDL_UpdateTexture` → window.
- **Data path**: the real `obd_data_cache`; fake data enters through the public
  setters. The mileage task (`vMileageDataStatisticTask`) really runs too
  (cooperative esp_timer emulation).
- **Events**: `app_event_recv` is always empty (single-threaded sim, no
  ESP-NOW/BLE producers).
- **NVS does not persist**; a relaunch returns to defaults.

## Known limitations

- Gear is given explicitly by the fake data, not derived from drivetrain ratios
  (`--profile` affects the profile name and profile-dependent decode logic)
- RaceChrono output, real OBD/BLE communication and OTA upload are unavailable
- Frame rate / touch feel have no relation to the device (QSPI 80MHz +
  CST816/9217); visual acceptance happens on real hardware
