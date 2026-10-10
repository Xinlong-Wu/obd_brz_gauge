# MX-5 NC / Circuit Instruments

English | [简体中文](README.md)

An MX-5 NC / JDM visual and interactive prototype for circular vehicle instruments, with 16 screens, 34 abnormal / system scenario options, and 78 exported artboards including wheel-position and unavailable-data variants. The master is 720×720, with a 360×360 actual-size preview; all data and connection actions are demonstrations. The imported design baseline is v10; the current warning-scenario version is v12.

## Editing and previewing

Use this directory for future changes. Edit `foundations.js` for shared rules, `components.js` / `components.css` for controls, `scenarios.js` for state scenes and sample readings, `nc-gauges.js` for screen composition and interactions, and `prototype.css` for the outer page layout. The preview server reads this directory; save, generate necessary assets, and refresh to see changes.

Run from the repository root. Only Python 3.9 or newer is required:

```bash
python3 ui-prototype/start-preview.py
```

Open <http://127.0.0.1:8769/prototype.html>. To open screen 05 directly, use <http://127.0.0.1:8769/prototype.html?page=oilpress>. Select “360 原尺寸” to check circular boundaries, spacing, and text overlap.

The scenario selector below the dial chooses individual or simultaneous warnings, with a separate retained choice for each screen. Screen 08 provides wheel selection; 09 / 10 / 11 have editable limits. The data toggle simulates vehicle disconnection on vehicle screens and independent heart-sensor disconnection on 11. Media and pairing have their own system scenes. Unavailable data takes precedence; restoring data resumes the selected warning scene. Open <http://127.0.0.1:8769/prototype.html?page=multi&scenario=multiple> for composite warnings.

The interactive component and foundation catalog is <http://127.0.0.1:8769/components.html>. See [DESIGN_SYSTEM.en.md](DESIGN_SYSTEM.en.md) for properties, states, circular constraints, and the editing workflow.

The service runs in the background. Repeated starts check which directory the service belongs to; to stop it or change ports:

```bash
python3 ui-prototype/start-preview.py --stop
python3 ui-prototype/start-preview.py --port 8770
```

`/__preview__` returns the actual served directory and process information to verify the preview source. Logs and PIDs are stored in `.preview/` and excluded from Git. Static files use `Cache-Control: no-store` to avoid stale screens. You can also open this directory's `prototype.html` directly in a browser; its resources are local, separate files.

## Screens

1. Gear / RPM: segmented RPM arc, central gear, auxiliary oil temperature and pressure.
2. Boost / vacuum: pressure arc, fine needle, large reading.
3. Coolant temperature: vertical column, large reading, and status.
4. Engine oil temperature: arc and reading share a center, with illustrative 40–140°C graduations.
5. Engine oil pressure: eccentric mechanical gauge on the left, independent reading and status on the right.
6. Three-channel monitor: main boost reading, oil temperature, and oil pressure.
7. Multi-channel monitor: oil pressure, oil temperature, voltage, coolant, boost, and TPS / B.P switching.
8. Tyre pressure and temperature: roadster illustration, four-wheel positioning, and low-pressure status.
9. Speed and cruise: current speed, cruise status, and set speed.
10. G-force: concentric circles, crosshairs, the current point, and both axis readings; optional reminders use a dashed threshold circle and LIMIT message.
11. Heart rate: current reading, low, peak, one-minute statistic, and waveform.
12. Mechanical pressure gauge: centered needle, 270° graduations, and independent pressure reading.
13. Music: track, artist, lyrics, progress, previous, play / pause, and next.
14. Day / night brightness: separate brightness memories, plus / minus, and mode switching.
15. Bluetooth and remote: connection state, discoverability, remote keyboard, pairing, and clear-pairings confirmation.
16. Pairing code: grouped code, confirmation, and back navigation.

## Current design constraints

- Warm white readings, amber accents, and red warnings; Barlow Condensed for numbers and IBM Plex Mono for labels.
- The music primary key is a 52px dark circle with a fine amber outline. Auxiliary icons have at least 44×44px hit areas; press feedback does not move controls.
- Screen 07 uses compact `TPS / B.P` labels and one combined hit area. Clicking labels, numbers, or whitespace switches the channel; amber and an underline mark the current channel.
- Screen 05 separates the eccentric gauge and digits, with pressure, unit, and status sharing the right alignment axis. Normal and warning readings have approximately 22px clearance from the gauge at 360 display size.
- Screen 12 places its title inside the scale ring, leaving only a small `BOOST` label. Needle and graduations are centered, with unboxed numbers.
- Screen 10 has a 1g inner circle and 2g outer circle, with the current point, both axis readings, and no trail. Triggered reminders show a threshold circle and LIMIT.
- Gauges omit the decorative `NC-01 / ROADSTER` signature; disconnected states retain `NO SIGNAL`.

## Exports and files

- `prototype.html` / `prototype.css`: multi-file page entry and styles.
- `nc-gauges.js`: pages, SVG rendering, and interactions; `scenarios.js`: scenes, sample readings, and warning reasons.
- `foundations.js`: foundation definitions; `foundations.css` / `foundations.json`: generated outputs.
- `components.js` / `components.css`: shared components and states; `components.html`: interactive catalog.
- `art.js`: base vector artwork; `fonts/`: local fonts and SIL OFL licenses.
- `vectors-text/`: 78 SVGs with text nodes and embedded fonts.
- `vectors/`: 78 outlined SVGs suitable for Figma import.
- `previews/`: 78 PNGs; `overview.png` / `overview.svg`: normal overview; `overview-alerts.png` / `overview-alerts.svg`: warning and system-state overview.
- `design-manifest.json`: page, state, color, and font index.

After editing sources, regenerate and commit the corresponding SVGs, PNGs, and overview. Browser preview needs no export dependencies. Exporting requires Node.js 20.9 or newer and Python:

```bash
cd ui-prototype
npm install
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r requirements.txt
npm run export
```

Export first generates foundation CSS / JSON, then runs `export-vectors.cjs`, `outline-type.py`, and `rasterize.cjs` in order. Paths are relative to this directory. Outlining Chinese lyrics requires a CJK font; the script finds common system fonts automatically, or you can provide a font file:

```bash
UI_CJK_FONT=/path/to/cjk-font.ttc npm run export
```

Check JS syntax, then inspect circular boundaries, digits, labels, and hit areas at 360 actual size. When editing interactions, verify the affected actions and normal / warning / disconnected states.


## Figma reference

<https://www.figma.com/design/4Ah7vldYa2qIrVheZ0Xzc8> is the early reference file with 6 core gauges. The complete current version is the prototype and SVGs in this repository.
