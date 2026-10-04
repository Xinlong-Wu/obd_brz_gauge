# Coding & Comment Style

English | [简体中文](CODE_STYLE.md)

Style rules for C code in this repository. They apply to **new code and
hand-written modules**; the goals are readability, testability and long-term
maintainability. The Chinese document is the source of truth; this file is a
1:1 mirror.

## Scope (read this first)

| Area | Applies? | Why |
|------|----------|-----|
| `main/export_path/ui.c` (generated part), `screens/*.c`, `ui_theme_generated.c`, `theme_assets/` | ❌ No | SquareLine/script-generated; re-export overwrites them (AGENTS.md red line) |
| `main/export_path/ui_ext.c`, `ui_disp_item.c` and other hand-written extensions | ✅ Yes | Hand-written; normalize as you touch them |
| `main/app_obd_dsp/`, `main/bsp_obd_dsp/`, `main/theme_engine/` | ✅ Yes | Hand-written business/BSP logic |
| `simulator/`, `tools/`, `tests/` | ✅ Yes | Tool and test code follow the same bar |

For existing code that predates these rules: **incremental conversion**, no
one-off rewrites — bring modules you touch up to standard and add their tests
in the same commit.

## Pure-logic header pattern (the core of testability)

Any "inputs → outputs, no side effects" logic (decoding, range checks,
stepping/smoothing, mask composition, layout math) goes into a `*_logic.h`
with everything `static inline`:

```c
/* ui_disp_item_logic.h — display-item pure logic (no LVGL / ESP-IDF deps) */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** Adaptive step: diff ≤ threshold moves by ±1; diff > threshold closes
 *  ~1/3 of the gap per tick. */
static inline int32_t ui_disp_item_anim_step_i32(int32_t displayed,
                                                 int32_t target,
                                                 int32_t threshold)
{
    ...
}
```

Rules:

- The header depends **only** on `<stdint.h>` / `<stdbool.h>` / other
  `_logic.h` files from this repo
- Never include LVGL, ESP-IDF, FreeRTOS or BSP headers — that is what lets
  `tests/` compile assertions against it directly (see
  `tests/test_ui_disp_item.c` for usage)
- The implementation file (`.c`) includes the header and calls it from the
  UI/IO layer; prefer migrating `static` implementations into `_logic.h`
  instead of leaving them in the `.c`
- Every new `_logic.h` ships with at least one unit test case (that is what
  makes later changes safe)

## Comments

- **Chinese comments by default**, explaining "why / constraints / pitfalls",
  never narrating what the code obviously does
- Module header: one block comment at the top of the file stating
  responsibility, boundaries and collaborators (see `simulator/src/main.c`
  and `bsp_stubs.c` for examples)
- Functions: public APIs use `/** ... */` Doxygen style — one Chinese
  sentence for the job plus key parameter/return constraints; small `static`
  helpers may skip the comment (the name is the comment)
- Numeric constants carry units (`0.1 bar`, `×100`, `ms`); magic numbers are
  either named or annotated with their origin
- Comments about external protocols/hardware timing cite the source
  (ISO PID, datasheet page, CAN frame id)
- No "translating the code" comments (`i++ // increment i`), no comments
  addressed at a reviewer

## Code organization

- Short, single-responsibility functions; split anything beyond ~60 lines
  (generated code exempt)
- Public APIs validate inputs defensively (NULL / ranges) and return early
  with clear semantics on bad input; `static` internals may rely on caller
  invariants without re-checking
- Naming: `snake_case`; consistent module prefixes (`disp_item_*`,
  `theme_*`, `espnow_link_*`); booleans prefixed `is_/has_/should_`; no
  information-free names like `Temp2` or `flag1`
- State lives in a `static` singleton struct (e.g. `s_ctx` / `s_st`) defined
  at the top of the file, not in scattered related globals
- Error paths: return and check `esp_err_t` on ESP-IDF; pair resource
  teardown (create/delete, mmap/munmap, lock/unlock) within the same
  function's view

## Per-module companion docs (optional)

Complex modules (state machines, drivers, protocol parsers) may add
`<module>.md` next to the source describing **implementation details**
(internal states, timing, known limitations). Division of labor with `docs/`:

- `<module>.md` = how it works (for people changing the code)
- `docs/*.md` = how to use it / source-of-truth lists (for users; governed by
  the AGENTS.md mapping table)

Never record the same fact in both places; when numbers disagree, code wins
and the docs get fixed.

## Commits & verification

- Conventional commits (`feat:` `fix:` `docs:` `build:` `chore:`); subject in
  English, body may be Chinese, explain "why"
- Behavior changes require three greens: unit tests (`ctest`), screenshot
  regression (`tools/sim_regress.py`; run `--update-goldens` first for
  intentional UI changes), firmware build (Docker)
- Documentation sync rules: see the mapping table in
  [AGENTS.md](../AGENTS.md)
