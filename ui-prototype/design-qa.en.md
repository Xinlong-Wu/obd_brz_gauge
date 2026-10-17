# Prototype visual acceptance

English | [简体中文](design-qa.md)

## v18 semantic typography (2026-10-17)

final result: passed

Scope covers all 17 screens and 61 scenes, plus unavailable data on 11 vehicle screens, a night example and both forget confirmations. Browser screenshots were checked at actual 360×360. Fonts, units, statuses and actions use 13 semantic roles, with necessary text at least 14px. Per-scene role evidence is stored in `.preview/typography/final-scenes.json`.

This pass captured actual before / after views and compared 05 / 07 / 15 side by side in `.preview/typography/before-after.png`. Intentional changes enlarge text, remove repeated explanations and separate main / composite / auxiliary value tiers, retaining JDM fonts, colors and button feedback. Normal and warning / system sheets were reviewed. Current content shows no visible text obstruction, circular clipping or button overflow; overlap of blank font line boxes is not treated as glyph obstruction.

Corrections addressed units near the car leaders on 08, title / coordinate-ring spacing on 10, the right statistic's circular clearance on 11, and endpoint ticks / labels on 12. Screen 05 reserves space for the unified primary value. Alert fields on 07 use concise copy with full reasons retained in semantic annotations. Both confirmation headings identify the target and preserve cancel / confirm actions.

Browser checks covered TPS / B.P switching with Enter, play / pause and tracks, brightness and day / night switching, discoverability disabling confirmation, simulated phone pairing and independent forgetting, and OBD discovery / selection / automatic completion and cancellation. The catalog shows every role and its warning state was checked; browser error logs are empty. All 24 prototype checks and `test_jdm_fonts` pass. New firmware semantic metadata changes no existing role, page or runtime allocation.

Regenerated 92 SVG / outlined-text / PNG sets and two overviews, preserving `data-type-role` and actual font weight. See [DESIGN_SYSTEM.en.md](DESIGN_SYSTEM.en.md) and `components.html#typography`. This remains browser design acceptance; viewing distance, brightness and rasterization on the physical screen require hardware rechecking.

## v17 connection-flow record

Date: 2026-10-16. Scope: screens 15 / 16 / 17 and the shared connection row in the v17 browser prototype. final result: passed

## Reference and comparison

The reference is the user-provided `codex-clipboard-96bc49af-9fe5-433c-9aec-fd90927d562f.png`, focusing on Frame 140's compact settings sections and Frame 151's pairing-code hierarchy. Both reference dials and actual browser screenshots were normalized to 360×360 and checked side by side in `.preview/connections/reference-comparison.png`.

Compact sections, code prominence and bottom Back navigation are retained. Intentional adaptations use the existing JDM fonts, warm white type, fine amber lines, and a Back control fully within the safe area. Brightness remains on 14; 15 separates OBD, phone and remote rather than copying every reference setting. The new list and status views on 17 use shared controls.

## Visual results

- Whole-row hit areas on 15 / 17 are 218×50px. The shared validator checks all corners against the R318 interactive safe area; every button is at least 44×44px.
- At 360 display size, checked the overview, phone code, discoverability off, failed / expired / completed phone pairing, OBD list, search / connection / reading progress, empty results, connection failure, waiting for the vehicle, disconnection, and both forget confirmations. Current text, units, buttons and circular boundaries show no visible overlap or overflow and use existing type and color roles.
- Compared typography, spacing, alignment, borders, backgrounds and copy in the reference comparison and state sheets. No new image assets; the Bluetooth graphic reuses the prototype's existing icon. Progress uses fine lines without full-screen flashing; pressing controls does not change their dimensions or position.

## Interaction and assets

- Browser checks covered phone discoverability and disabled confirmation, simulated pairing completion, discovery leading to a device list, selection of another adapter and automatic data-ready progression, retry entries, cancellation / Back, and forgetting OBD while preserving phone and remote settings. Enter works on the shared connection row; the component catalog has no console errors.
- Inspector-selected progress remains static. Leaving the page cancels automatic progress, preventing stale callbacks from overwriting binding state. All 20 `npm test` checks pass, including connection isolation, cancelled replacement and recovery semantics. Full export generates 92 SVG / outlined-text / PNG sets and two overviews with adaptive height.
- Reviewable exports: [connection overview](previews/bluetooth.png), [phone pairing](previews/pair.png), [OBD list](previews/obd.png). Actual browser evidence is stored in `.preview/connections/` (excluded from Git).

## Scope limits

Acceptance covers only the browser prototype and current sample content. BLE discovery, binding, phone authorization and vehicle data are not integrated; firmware pages and interactions are unchanged. Future long device names, larger result sets and actual connection timeouts require separate design and verification.
