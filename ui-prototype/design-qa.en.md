# Connection-flow visual acceptance

English | [简体中文](design-qa.md)

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
