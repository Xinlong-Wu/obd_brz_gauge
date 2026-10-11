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

## Page navigation (16-page carousel)

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
| 8 | TYRE MONITOR | roadster outline + four tyre pressures |
| 9 | SPEED | perspective roadside animation (faster with speed, stops when stopped) + cruise state |
| 10 | G-FORCE | 1g/2g rings + current point |
| 11 | DRIVER PULSE | heart rate & trend (source not wired yet; NO SENSOR state) |
| 12 | PRESSURE / ANALOG | 41-tick mechanical boost dial |
| 13 | NOW PLAYING | media page (source not wired yet; disconnected state) |
| 14 | ILLUMINATION | day/night brightness memory and adjustment |
| 15 | CONNECTIONS | adapter status and binding management |
| 16 | PAIR DEVICE | pairing code |

## Day / night & brightness (ILLUMINATION)

- **DAY / NIGHT brightness** is remembered separately (defaults 85% / 35%);
  **−/+** steps by 5 within 10–100%, out-of-range buttons disable
  themselves; changes apply immediately and persist in NVS
- The bottom button toggles **DAY ACTIVE / NIGHT ACTIVE**; in night mode all
  primary text switches to the night amber take-over and the night backlight
  level applies

## Bluetooth (CONNECTIONS / PAIR DEVICE)

- The status chip shows adapter state: **DEVICE CONNECTED** (green, bound and
  connected) / **CONNECTION LOST** (amber, bound but not connected) /
  **NO DEVICE** (unbound)
- **FORGET** clears the adapter binding (name + MAC); **PAIR** opens the
  pairing-code page — the code is derived from the device MAC; enter it in
  the companion app to bind
- The **DISCOVERABLE / REMOTE KEY** toggles are UI placeholders for now
  (firmware capability pending)

## Data & disconnect

All data flows through the unified channel reading the OBD data cache
(BLE / ESP-NOW dual source, thread-safe). When disconnected: values show
**—**, live indicators hide, and each page shows its own
**NO SIGNAL / WAITING / —** semantics; everything resumes automatically.

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
