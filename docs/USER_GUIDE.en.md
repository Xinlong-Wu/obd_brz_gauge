# User Guide

English | [简体中文](USER_GUIDE.md)

For flashing and building see [FLASH.en.md](FLASH.en.md); if you cannot
connect, start with [TROUBLESHOOTING.en.md](TROUBLESHOOTING.en.md).

## First boot

1. On power-up the Sky Gauge logo appears, then the boot animation set in
   settings plays (RACE by default, can be disabled)
2. On first use open **Settings** (MENU tile → SETTINGS) and pick your car
   with the **VEHICLE** roller (applies immediately, no reboot)
3. Back on **MENU → BLE SCAN**, select your ELM327 BLE adapter
4. Once connected the gauge initializes automatically (protocol auto-detection
   is described in [TROUBLESHOOTING.en.md](TROUBLESHOOTING.en.md#protocol-auto-detection))
   and data starts flowing

On every later power-up the gauge reconnects to the last adapter
automatically (remembers BLE name + MAC) and self-heals when data stalls —
normally **you just power on and drive**, no manual steps.

## Page navigation (dynamic dashboard)

The main UI is a **user-definable pager**; swipe left/right to move between
tiles —

```
   MENU → [gauge pages 1..N, fully editable] → ADD
```

- **MENU tile**: vehicle name plus three entry buttons — **BLE SCAN**
  (connect an OBD adapter; on slave gauges this doubles as the FIND MASTER
  scan), **SETTINGS**, and **INFO / OTA** (the version page: build tag,
  connection status, OTA button and the hidden entries)
- **Gauge pages** (initially six, migrated automatically from your previous
  TEMP/INFO/CHART/NEEDLE mapping with the same look):
  - **METRIC pages**: 1–6 data cards, any of the 18 unified channels
    (coolant/oil/intake temp, RPM, speed, oil pressure, g-force, TPMS, ...)
  - **GEAR page**: big gear digit (R/N supported; 0x141 direct read wins,
    ratio-derived fallback) + RPM arc
  - **G-FORCE page**: g-force dot plot (lateral/longitudinal, 0.01 g)
- **ADD tile**: tap "+" to append a new gauge page (8-page cap), defaulting
  to a single RPM slot

**Long-press any gauge page → edit mode** (paging locked, dimmed overlay
with three zones):

- **EDIT** (top-left) → the config page: TYPE (METRIC / GEAR / G-FORCE);
  METRIC pages additionally expose SLOTS (1–6) and per-slot CHANNEL (an
  18-item roller); every change persists immediately
- **DELETE** (top-right) → remove the page (one page minimum), landing
  back on MENU
- **BACK** (bottom band) → leave edit mode

Swipe left/right leaves the config page; when data times out or BLE drops,
gauge pages show a **NO SIGNAL** banner.

**Version page** (via MENU → INFO / OTA): swipe up → BLE SCAN, swipe down →
Settings, OTA button → OTA update mode (see
[FLASH.en.md](FLASH.en.md#app-ota-upgrade-dual-slot-rollback)).

**Hidden entries** (unchanged):

- **Double-tap the logo page** → OBD protocol roller (**long-press 2 s**
  saves and reboots; protocol 0 = auto-detect)
- **10 rapid taps on the version page** → showroom demo mode

> The legacy static carousel (GEAR→RPM→…→version ring) is superseded by the
> dynamic pages; its data mappings migrate into the initial gauge pages on
> first boot. When a partition theme with pages is loaded, boot still lands
> on the theme gauge page (theme pages ↔ version page swipe loop).

## Settings page

| Setting | Description |
|---------|-------------|
| VEHICLE | The 17 vehicle profiles; switching applies immediately (affects gear detection, oil-temp strategy, protocol lock, …)|
| THEME | Compiled-in themes (default / amber / ocean plus community themes); selecting **reboots** the device to apply |
| Brightness | 10–100% |
| OBD POLL | Poll tier NORMAL / FAST / TURBO (default slot gap 30 / 15 / 5 ms; takes effect on the next poll cycle, no reboot). Vehicles that pin their own poll gap (ZN/C6 CAN, MX-5, ...) are unaffected; TURBO can overwhelm cheap clone adapters — drop back to NORMAL if data gets unstable |
| RACECHRONO | ON = full BLE services (RaceChrono + pairing + OTA); OFF = minimal mode (Info + OTA only, no advertising) to save memory |
| Swipe down → MULTI-GAUGE | Triple-gauge role setting (see below)|

Everything is persisted in NVS. Trip / mileage statistics accumulate per
power cycle only and reset on reboot.

## Triple-gauge cluster (ESP-NOW)

An ELM327 adapter accepts a single BLE client, so multiple gauges work as
**one master plus slaves**:

- **Master (MASTER)**: keeps the BLE + ELM327 connection and reads OBD,
  broadcasting parsed data over ESP-NOW
- **Slave (SLAVE)**: receives the broadcast and renders, zero extra OBD load
- **Standalone (default)**: ESP-NOW disabled

All gauges run the same firmware; the role is picked under
**Settings → swipe down → MULTI-GAUGE** (takes effect after reboot).

**Pairing** (real BLE pairing, not blind broadcast binding):

1. Set the master to MASTER; it advertises `SkyGauge-XXYY` (device suffix, so
   multiple cars at a track day don't interfere)
2. Set slaves to SLAVE; an unpaired slave boots straight into the
   **FIND MASTER** scan page
3. Pick the master to follow from the list — done (the master's MAC is
   remembered and auto-joined on every later boot)

The cluster also supports a **linked RPM warning**: past the threshold the
three gauges light up in position order and then all flash (toggleable as
Linked mode on the RPM warning page), plus the synchronized "RACE / AS / ONE"
boot animation.

## Boot animation

Three modes (NVS `intro_enable`):

| Mode | Effect |
|------|--------|
| OFF | Skip the animation, go straight to the gauges |
| RACE | Built-in "RACE AS ONE" animation (cluster-synchronized)|
| VIDEO | Plays a custom animation from the `bootmedia` partition (upload via the phone app, see [FLASH.en.md](FLASH.en.md#app-ota-upgrade-dual-slot-rollback))|

Custom animation requirements: 360×360 canvas, 1:1 aspect, mind the corner
clipping on the round screen. Updates are transactional on the device — an
interrupted upload never damages the existing animation.

## Updating from the phone app

The companion app (distributed separately, not in this repo) can push three things; each
requires the gauge to be in OTA mode first (**version page → OTA button**):

- **Firmware**: BLE handshake → WiFi hotspot (`OBD-Gauge-OTA-XXXX`, password
  `obd2024`) for fast transfer, written to the inactive OTA slot; the new
  firmware must pass a 15-second boot self-check before being marked valid,
  otherwise the bootloader rolls back
- **Boot animation**: trim the video in the app, encode, upload
- **Theme package**: pushes a runtime theme to the theme_0 partition
  (see [THEMES.en.md](THEMES.en.md))

The device exposes a read-only BLE manifest service (`0x1FFA`) with hardware
and build info; the app validates compatibility before flashing and refuses
mismatched hardware. Protocol details in [APP_PROTOCOL.en.md](APP_PROTOCOL.en.md).

## Optional external sensors

| Sensor | Wiring | Notes |
|--------|--------|-------|
| RS485 brake temperature | GPIO13 (TX) / GPIO12 (RX), 9600 baud, Modbus RTU | Data feeds the CHART page; brake-temp alarms throttled to one per 30 s |
| ADS1115 oil pressure | I2C, V1 Waveshare board only | Cars such as Supra A90 / BMW E read oil pressure over OBD DIDs instead |

See the matching sections of [TROUBLESHOOTING.en.md](TROUBLESHOOTING.en.md).
