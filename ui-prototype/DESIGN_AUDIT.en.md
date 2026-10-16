# Reverse audit of the 16 screens

English | [简体中文](DESIGN_AUDIT.md)

Audit date: 2026-10-10. The target is the current v16 working copy of repository `ui-prototype`, checked against [DESIGN_SYSTEM.en.md](DESIGN_SYSTEM.en.md), `foundations.js`, and `scenarios.js`.

## v17 connection-flow addendum (2026-10-16)

New screen 17 discovers / selects / connects OBD; 15 is the connection overview and 16 explicitly handles phone pairing. The current set has 17 screens, 61 scene options (44 additional scenes), and 92 exported artboards; all 20 Node checks pass. The new connection flow was checked at 360×360 in the browser for text, circular boundaries, normal / progress / failure / waiting / disconnected states, and independent forget confirmations, including whole-row click, keyboard operation and progress cancellation. Progress examples in the scenario inspector remain static; actual buttons trigger the automatic demonstration.

This is a focused addendum to the original v16 audit. The 16-screen table below remains historical; the entire vehicle-screen browser tour was not repeated in this pass. See [design-qa.en.md](design-qa.en.md) for the visual comparison.

## v16 conclusion

**The specified warning, advisory, and system scenarios are implemented, and current samples pass the checks.** The 16 screens expose 50 scene options, including 34 abnormal or system scenes. Each of the three single-wheel scenarios on 08 supports FL / FR / RL / RR. Exports include 78 artboards and separate normal / warning overviews.

Warnings combine readings with reasons. Shift advice, custom limits, and heart-rate range reminders use amber; monitored abnormalities use red. Critical states such as low oil pressure, excessive RPM, and extremely low tyre pressure add thin red rim arcs. Screen 14 retains ordinary brightness settings without numeric-threshold warnings.

## Coverage

- Inspect all scene options at 360×360 in the browser, plus all four wheel positions for each single-wheel scenario on 08. Check messages, units, hit areas, and circular boundaries; empty font layout-box space is not treated as painted-glyph obstruction.
- Disconnect each of the 11 vehicle screens while in warning state: active graphics and warning messages disappear; restoring data retains the selected scene. Heart rate on 11 has an independent unavailable-data state and is unaffected by vehicle disconnection.
- Verify limit inputs on 09 / 10 / 11, and control states and recovery paths for unavailable media, connection loss, failed pairing, and expired codes. The combined toggle on 07 and normal play / track switching on 13 retain existing interactions.
- All 16 `npm test` checks pass, covering foundation synchronization, component geometry, state semantics, all-scene rendering, disconnected precedence, simultaneous-warning ordering, four-wheel selection, optional limits, system-disabled states, motion frequency, and perspective boundaries.
- Regenerate SVGs, outlined type, PNGs, normal / warning overviews, and manifest. Browser preview reads the repository directory.

## Results by screen

`PASS_SCENARIOS` means current normal state and listed scenes pass; `PASS_NORMAL` means the settings screen needs no numeric warning and normal interaction passes. Scene IDs match the code.

| Number | Screen | Result | Checked scene IDs |
|---|---|---|---|
| 01 | GEAR & RPM | PASS_SCENARIOS | shift / rpm-high / oil-hot / oil-low |
| 02 | MANIFOLD | PASS_SCENARIOS | pressure-high |
| 03 | COOLANT | PASS_SCENARIOS | coolant-hot |
| 04 | OIL TEMPERATURE | PASS_SCENARIOS | oil-hot |
| 05 | OIL PRESSURE | PASS_SCENARIOS | oil-low |
| 06 | POWERTRAIN | PASS_SCENARIOS | pressure-high / oil-hot / oil-low / multiple |
| 07 | ENGINE VITALS | PASS_SCENARIOS | oil-low / oil-hot / coolant-hot / voltage-low / voltage-high / pressure-high / multiple |
| 08 | TYRE MONITOR | PASS_SCENARIOS | tyre-low / tyre-high / tyre-hot / multiple |
| 09 | SPEED | PASS_SCENARIOS | speed-limit |
| 10 | G-FORCE | PASS_SCENARIOS | g-limit |
| 11 | DRIVER PULSE | PASS_SCENARIOS | heart-high / heart-low / no-data |
| 12 | PRESSURE / ANALOG | PASS_SCENARIOS | pressure-high |
| 13 | NOW PLAYING | PASS_SCENARIOS | media-unavailable |
| 14 | ILLUMINATION | PASS_NORMAL | normal |
| 15 | CONNECTIONS | PASS_SCENARIOS | connection-lost / no-device |
| 16 | PAIR DEVICE | PASS_SCENARIOS | pair-failed / pair-expired |

The `tyre-low` / `tyre-high` / `tyre-hot` scenes on 08 support all four independent wheel positions. `multiple` uses FL low pressure, RR high pressure, and RL high temperature. Simultaneous warnings on 06 / 07 have a severity-ordered summary and individually marked abnormal readings.

## Prior findings addressed

- **I01 / I03 / I04**: Needles, active arcs, RPM segments, temperature columns, and progress bars now follow scenes and disappear when disconnected. Tyre temperatures clear, active cruise state ends, and live limit messages are suppressed without data.
- **I05**: The divider and lower partitions on 06 move down, leaving the main `bar` unit in the primary region without a crossing divider.
- **I06**: The song title on 13 uses the registered `mediaTitle` type role; static graduation sizes bind to `support`.
- **I07**: Screen 11 gains high / low heart-rate reminders and independent unavailable data, with an editable range. Messages refer to custom ranges, not health diagnoses.
- **I02**: The needle sweeping over printed names on 02 is accepted mechanical-dial layering and is retained. Warning needle position is calculated from the sample pressure.

## v16 visual revisions

- Remove exclamation marks from all warning text and wheel labels, retaining reasons, colors, and fine-line markers.
- On 03, remove the separate red column segment, mark 110–120°C graduations and the 120 label red, and keep the column and marker driven by the sample temperature.
- On 10, remove bottom LAT / LON and repeated numeric rows. Normal state shows the point and coordinate plot; only a dashed threshold circle and emphasized current point convey a triggered reminder, without bottom reminder text or threshold numbers.

## Road-scene motion checks on 09

The speed screen adds trees and buildings on both sides while retaining a clear primary reading. Trees use muted gray-green color; buildings use gray outlines and a few dim warm windows. Browser checks show approximately a 1:3 motion-frequency ratio at 40 / 120 km/h, and frozen perspective graphics at zero speed or disconnection. The preview slider adjusts speed; speed-limit reminders trigger only when enabled and above the configured threshold. Speed changes retain accumulated displacement. Static exports use a representative frame; the interactive webpage retains motion.

## Scope of guarantees

Checks cover defined scenes and input ranges, representing prototype design states rather than implemented vehicle warning logic. New numeric formats, long song titles, long status messages, or new graphics require their own checks. The prototype is not a layout guarantee for all future data. Unit tests do not replace visual checks.

## Preview and future editing

Open <http://127.0.0.1:8769/prototype.html?page=multi&scenario=multiple>. Select a scene below the dial; choose wheels on 08 and edit reminder limits on 09 / 10 / 11. Shared alert samples are at <http://127.0.0.1:8769/components.html#alerts>.

Edit scenes and sample values in `scenarios.js`, foundation roles in `foundations.js`, shared presentation in `components.js`, and composition in `nc-gauges.js`.
