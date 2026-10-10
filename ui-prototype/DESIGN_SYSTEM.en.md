# Components & foundations

English | [简体中文](DESIGN_SYSTEM.md)

This specification uses the current repository prototype as its source of truth. It follows the early Figma Foundations, Readout, Action, and Gauge Shell structure, adding the recently accepted controls and circular-screen rules. Interactive examples are in [components.html](components.html), available at <http://127.0.0.1:8769/components.html>.

## Layers and source of truth

- `foundations.js`: color roles, type roles, sizes, circular geometry, spacing, strokes, and hit sizes.
- `foundations.css` / `foundations.json`: generated browser variables and readable data; do not edit manually.
- `components.js` / `components.css`: shared SVG components, properties, states, and feedback.
- `nc-gauges.js`: composition and interactions for 16 screens; coordinates define each layout.
- `art.js`: base vector artwork with semantic color and label-font bindings to the foundations.
- `components.html` / `catalog.js`: an interactive specification using the actual shared components to inspect sizes and states.

Edit shared foundations and controls in their corresponding layers. Specific graduations, coordinate plots, vehicle illustrations, and partitioned layouts remain in the screen and artwork layers.

## Foundation rules

### Color

- `text` / `muted`: primary readings and supporting labels.
- `amber`: selection, emphasis, and the current item, combined with an underline, outline, or text.
- `red`: warnings; the Readout warning state always uses the warning color.
- `green` / `blue`: normal status and connection information.
- `night`: nighttime reading color; day / night brightness is a separate interaction state.
- `face` / `panel` / `line`: dial background, control surface, and structure lines; the outer page panel has a separate viewer role.

Do not communicate state through color alone. Warnings need readings or reason text. Selections need a marker, and unavailable data uses `—` and status information.

### Type and size

Numbers use Barlow Condensed; labels, units, and statuses use IBM Plex Mono. Chinese lyrics use a system font.

- `hero`: 180 master pixels, displayed at 90px.
- `eccentric`: 144, displayed at 72px.
- `analog`: 96, displayed at 48px.
- `compact`: 76, displayed at 38px.
- `mediaTitle`: 60, displayed at 30px, reserved for the music screen song title.
- `title` / `label` / `support` / `small`: 28 / 26 / 24 / 22, displayed at 14 / 13 / 12 / 11px.

Type sizes and families are defined in one foundation file. Prefer existing roles and explain new ones. Avoid copying a separate color and type scale into each screen.

### Circular screen and spacing

- The master is 720×720, the reference display is 360×360, and the center is (360,360). Coordinates, sizes, and radii use master pixels.
- The content-check radius is 342. All four corners of each control's hit area must remain within the interactive radius of 318.
- Hit areas are at least 88×88 master pixels, displayed at 44×44px. Components reject undersized, non-finite, and out-of-bounds regions.
- Shared spacing roles are 8 / 16 / 24 / 32 master pixels. Independent gauge and readout regions should have at least 32 master pixels, displayed at 16px, between them.
- Components must not cover other data regions. Check the maximum digit count, long labels, warning, and disconnected states at 360 actual size.
- A mechanical needle sits above printed dial text and may sweep over names, units, or graduations. This layering is not by itself a layout defect. Separate numerical readouts, status messages, and controls must remain legible without sustained obstruction. The current layering of the fine needle and printed names on 02 is retained.

## Components and properties

### Readout

Properties include `x`, `y`, `label`, `value`, `unit`, `size`, `state`, and an optional normal-state accent `color`.

- `size`: `primary`, `compact`, `eccentric`, `analog`.
- `state`: `normal`, `warning`, `disconnected`.
- Labels, values, and units share one center axis; each size has fixed vertical offsets.
- `warning` forces the warning color. `disconnected` shows `—` instead of the provided value.

```js
const U = NC_COMPONENTS;
U.readout({
  x: 200, y: 220, label: 'OIL TEMP', value: '104', unit: '°C',
  size: 'primary', state: 'normal'
});
```

Figma's `Size / State` and `Label / Value / Unit` map to these size, state, and text properties. The `metric(...)` adapter accepts a final `state` argument; unavailable data takes precedence over warnings.

### Action and auxiliary controls

Action properties include position, width, label, action name, `variant`, and `disabled`.

- `variant`: `secondary`, `primary`, `ghost`, `danger`.
- Disabled state is separate from visual variants, with `aria-disabled` and `tabindex=-1`; disabled controls ignore clicks and keyboard input.
- Press, hover, and keyboard focus only change color, stroke, or visible markers. Controls do not translate or scale. Feedback lasts 120ms and supports reduced-motion preferences.
- The music primary key is a 52px circle; auxiliary track icons retain 44px hit areas.
- Toggles synchronize the thumb, ON / OFF label, and `aria-checked`.
- TPS / B.P uses one combined hit area, with amber and an underline marking the current item.

```js
U.actionButton({
  x: 240, y: 260, width: 240, label: 'CONFIRM', act: 'confirm',
  variant: 'primary', disabled: false
});
```

`roundButton`, `mediaButton`, `toggleControl`, and `readoutControl` are specialized controls in the same shared layer. Screens still receive actions through `data-act`; Enter / Space and click invoke the same action.

### Gauge Shell and screen composition

`shell()` shares the dial background, circular rim, and top marker. Screen clipping and center alignment follow the foundation geometry. The bottom omits decorative signatures; screens provide necessary disconnected status.

Screen composition keeps distinct presentations: centered main gauges on 01 / 02 / 04 / 12, the eccentric split on 05, multi-channel partitions on 06 / 07, and the concentric coordinate plot on 10. Shared components keep type, readings, and controls consistent while composition suits each data type.


## Editing and verification

After editing foundations, generate CSS and JSON, then validate component contracts. Full export also regenerates screen assets:

```bash
cd ui-prototype
npm run export:foundations
npm test
npm run export
```

Component tests cover foundation synchronization, 44px hit areas, circular boundaries, disabled state, Readout warning / disconnected semantics, artwork color bindings, and whether every screen renders under the main settings. Browser checks are still required for screen graphics and actual layout.


In the browser, check state switching and controls in the component catalog, then inspect affected screens at 360 actual size for labels, digits, units, circular boundaries, and interactions. Intentional visual changes require updated SVGs, PNGs, overview, and both language specifications.
