#pragma once
#ifndef LV_CONF_H
#define LV_CONF_H 1
/* Simulator LVGL 9.6 config — mirrors the firmware's sdkconfig LV_* settings
 * (RGB565 big-endian display pipeline, montserrat 12/14/16/26/32/48 enabled,
 * default font 16, libc alloc).
 *
 * Byte order: the UI image C arrays are RGB565_SWAPPED (big-endian, same bytes
 * as the v8 LV_COLOR_16_SWAP output) and the theme partition assets match; the
 * SW renderer converts them at draw time, so the display itself runs NATIVE
 * RGB565 and the SDL blit needs no swapping.
 *
 * Widget set matches what main/export_path actually instantiates:
 * arc/bar/button/canvas/chart/image/label/list/roller/slider/spinner (+obj)
 * plus the in-tree lv_meter port (main/ui_widgets/lv_meter, config-free).
 * Everything else stays off so accidental usage fails loudly at compile time. */

/* Memory / string / sprintf: plain libc on PC. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Color: native RGB565 display; SW renderer must accept the big-endian
 * RGB565 image sources used across the generated UI assets. */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 1

/* Tick source: simulator clock (real = SDL millis, virtual = frame-locked for
 * deterministic screenshots). LVGL v9 takes it via lv_tick_set_cb() at startup
 * (src/main.c); the definition lives in the sim binary. */

/* Timings matching the firmware sdkconfig. */
#define LV_DEF_REFR_PERIOD 16

/* Fonts (firmware set; the UI mostly uses its own FontTypoder fonts). */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_FONT_FMT_TXT_LARGE 1

/* Widgets used by main/export_path. */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_CANVAS 1
#define LV_USE_CHART 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_LIST 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SPINNER 1

/* Not instantiated by any screen, but ui_helpers.c (SquareLine-generated
 * generic helpers) references dropdown/textarea/keyboard/spinbox — the
 * firmware enables all widgets, and so do we. Keyboard/textarea need
 * buttonmatrix internally. */
#define LV_USE_DROPDOWN 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_SPINBOX 1
#define LV_USE_BUTTONMATRIX 1

/* Simulator-panel widgets (not used by the firmware UI, but by the second
 * LVGL display that renders the data-adjustment panel). */
#define LV_USE_SWITCH 1

/* Widgets NOT used by the UI — kept off so stray usage fails at build. */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_CHECKBOX 0

/* Themes (app_main.c calls lv_theme_default_init(); ui.c does the same). */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* Debug: asserts like the firmware, logging off. */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_ASSERT_HANDLER_INCLUDE "assert.h"
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0

#endif /* LV_CONF_H */
