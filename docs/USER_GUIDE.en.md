# User Guide

English | [简体中文](USER_GUIDE.md)

For flashing and building see [FLASH.en.md](FLASH.en.md); if you cannot get
data, start with [TROUBLESHOOTING.en.md](TROUBLESHOOTING.en.md).

> **WS128 (1.28", no touch panel) users**: every interaction in this guide
> (navigation, brightness, the Bluetooth page) is touch-based. The WS128 board
> has no touch, so the firmware runs **display-only** — boot, gauge pages and
> BLE auto-reconnect to the previously bound adapter all work, but you cannot
> swipe between pages. Use **WiFi screen capture** with **remote touch** for
> full control (hotspot starts automatically at boot; see "WiFi screen
> capture" below). The 1.28" **touch variant** (WS128T) has full capacitive
> touch — everything in this guide works as written.

## First boot

1. Power on shows the **SKY GAUGE** splash (~1.2 s), then the **GEAR / RPM**
   page
2. If an ELM327 adapter is already bound in NVS (paired earlier, or written
   with an NVS tool), the gauge reconnects automatically and data starts
   flowing; stalls self-heal — **just power on and drive**
3. When unbound, pages still render with **—** placeholders (disconnected
   semantics); see "Bluetooth" below for the binding path

## Page navigation (17-page carousel)

The main UI is a **horizontal carousel**: **swipe left/right** to cycle
through 16 pages —

| # | Page | Content |
|---|------|---------|
| 1 | GEAR | 56-segment RPM arc + big gear + oil temp/pressure summary |
| 2 | MANIFOLD | 270° pressure arc + fine needle, −1..3 bar |
| 3 | COOLANT | vertical thermal bar (60–120 °C, red zone from 110 °C) + status |
| 4 | OIL TEMPERATURE | 270° oil-temp arc + primary readout (40–140 °C) |
| 5 | OIL PRESSURE | eccentric mechanical dial + right-side readout (0–6 bar) |
| 6 | POWERTRAIN | boost bar + oil temp/pressure compact readouts |
| 7 | ENGINE VITALS | four corner readouts + TPS / B.P switcher (tap to toggle) |
| 8 | TYRE MONITOR | roadster outline + four tyre pressures (≤0.5 bar red low-pressure alert / ≥3.4 bar amber high, footer summary) |
| 9 | SPEED | perspective roadside animation (faster with speed, stops when stopped) + cruise state |
| 10 | G-FORCE | 1g/2g rings + current point (dashed limit circle appears and the dot turns amber above 1.0 g) |
| 11 | DRIVER PULSE | heart rate & trend (source not wired yet; NO SENSOR state) |
| 12 | PRESSURE / ANALOG | 41-tick mechanical boost dial |
| 13 | NOW PLAYING | media page (source not wired yet; disconnected state) |
| 14 | ILLUMINATION | day/night brightness memory and adjustment |
| 15 | CONNECTIONS | adapter status and binding management |
| 16 | PHONE PAIRING | phone pairing: discoverable switch + pairing code (MAC-derived); confirming enters the done state (PAIR NEW / FORGET) |
| 17 | OBD SETUP | OBD adapter: device list → connection progress (SEARCHING / CONNECTING / READING VEHICLE) → DATA READY / CONNECTION LOST; FORGET asks for confirmation and really clears the binding |

## Day / night & brightness (ILLUMINATION)

- **DAY / NIGHT brightness** is remembered separately (defaults 85% / 35%);
  **−/+** steps by 5 within 10–100%, out-of-range buttons disable
  themselves; changes apply immediately and persist in NVS
- The bottom button toggles **DAY ACTIVE / NIGHT ACTIVE**; in night mode all
  primary text switches to the night amber take-over and the night backlight
  level applies

## Bluetooth (CONNECTIONS / PAIR DEVICE)

- The status chip shows adapter state: **DEVICE CONNECTED** (green, bound and
  connected) / **CONNECTION LOST** (amber, bound but not connected; the footer
  shows SAVED PAIRING RETAINED) / **NO DEVICE** (unbound)
- **FORGET ALL** clears the adapter binding (name + MAC) — tapping it opens a
  **confirmation dialog**; only **CLEAR** actually clears, **CANCEL** goes back.
  **PAIR** opens the pairing-code page — the code is derived from the device
  MAC; enter it in the companion app to bind
- **BACK** on this page returns to the gear / RPM page (page 1)
- The **DISCOVERABLE / REMOTE KEY** toggles are UI placeholders for now
  (firmware capability pending)

### Connection design in the browser prototype (v17)

The connection flow of the [browser prototype](../ui-prototype/README.en.md#connection-flow) (15 connections overview / 16 phone pairing / 17 OBD setup) is now synced to the firmware: phone pairing codes and OBD connections are separate, each with its own forget confirmation. The OBD device list and connection progress mirror the prototype demo flow (the firmware has no BLE scan source yet); FORGET really clears the NVS binding.

Browser prototype v18 additionally unifies all 17 screens by semantic text roles, enlarges labels, units, statuses and buttons, and removes repeated explanations. Main / composite / auxiliary readings use consistent separate tiers. Firmware mirrors only the new semantic constants; existing page font mappings have not switched.

## Data & disconnect

All data flows through the unified channel reading the OBD data cache
(BLE / ESP-NOW dual source, thread-safe).

**Channel alerts**: on threshold breach the footer shows a reason summary
(multiple items as "first +N"); critical alerts additionally draw double red
arcs on the dial rim and turn the related readouts red. Thresholds:
coolant ≥110°C, oil temp ≥130°C (red alert — the dial red zone
130–140), oil pressure <1.0 bar (red alert), boost ≥2.4 bar,
RPM ≥6800 (shift prompt) / ≥7000 (red alert — the dial red zone 7000–8000,
entering it trips the alarm), voltage ≤10.8 / ≥16.4 V, tyre pressure ≤0.5
(red) / ≥3.4 bar, G-force >1.0 g (dashed limit circle + footer reminder).

When disconnected: values show **—**, live indicators hide, alert summaries
are suppressed, and the dial bottom shows amber **NO SIGNAL** (NO SENSOR on
the heart page); everything resumes automatically.

## Known trims (vs the legacy UI)

The legacy settings page, OBD protocol selector, theme system, alarm
threshold pages, custom gauge pages and the OTA entry page were removed with
the JDM UI rewrite: vehicle/poll settings keep their NVS values, and the OTA
HTTP backend is kept but currently unreachable from the UI (see
[FLASH.en.md](FLASH.en.md)). These will return in the JDM design language
over time.

## WiFi screen capture (remote view / screenshots)

The firmware can stream its screen over WiFi while the gauge keeps working
(BLE/ESP-NOW uninterrupted):

1. **WS128 and other no-touch builds**: the hotspot starts at boot
   (`OBD_SCREENSHOT_AUTO_START`, on by default in ws128 builds); touch builds
   can enable the same Kconfig option via menuconfig
2. Join hotspot `OBD-Gauge-View-XXXX` (password `88888888`)
3. Open `http://192.168.4.1:8080/` — live view (~12 fps MJPEG) plus
   "download current frame JPG" and "download exact-color BMP" buttons;
   or `curl -o shot.bmp http://192.168.4.1:8080/screenshot.bmp`

**Remote touch** (`OBD_REMOTE_TOUCH`, on by default in ws128 builds): the
control page forwards **swipes/taps** to the gauge — page turns, brightness,
the Bluetooth page, all identical to real touch.

The device has no internet while joined to the hotspot; switch back to your
WiFi afterwards. Endpoint details: [APP_PROTOCOL.en.md](APP_PROTOCOL.en.md).

## External sensors (optional)

| Sensor | Wiring | Notes |
|--------|--------|-------|
| RS485 brake temp | GPIO13 (TX) / GPIO12 (RX), 9600 baud, Modbus RTU | feeds the unified channel |
| ADS1115 oil pressure | I2C, V1 Waveshare board only | Supra A90 / BMW E etc. can read oil pressure via OBD DID instead |

Troubleshooting: the matching sections in
[TROUBLESHOOTING.en.md](TROUBLESHOOTING.en.md).
