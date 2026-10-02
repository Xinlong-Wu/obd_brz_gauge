#pragma once
#ifndef LV_CONF_H
#define LV_CONF_H 1
/* Simulator LVGL config — mirrors the firmware's sdkconfig LV_* settings
 * (CONFIG_LV_COLOR_DEPTH=16, CONFIG_LV_COLOR_16_SWAP=y, LV_MEM_CUSTOM=malloc,
 * montserrat 12/14/16/26/32/48 enabled, default font 16).
 *
 * LV_COLOR_16_SWAP MUST stay 1: the UI image/font C arrays were generated in
 * big-endian byte order (see tools/convert_rpm_flash.py output header) and the
 * boot-block decoder assumes it. The SDL flush callback swaps bytes back
 * before blitting.
 *
 * Widget set matches what main/export_path actually instantiates:
 * arc/bar/btn/canvas/chart/img/label/list/meter/roller/slider/spinner (+obj).
 * Everything else stays off so accidental usage fails loudly at compile time. */

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/* Memory: plain libc malloc, no pool sizing needed on PC. */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "stdlib.h"
#define LV_MEMCPY_MEMSET_STD 1

/* Tick: SDL millis, so no separate tick thread is needed. */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "SDL.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)SDL_GetTicks())

/* Timings matching the firmware sdkconfig. */
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_DPI_DEF 130

/* Drawing */
#define LV_DRAW_COMPLEX 1
#define LV_GRADIENT_MAX_STOPS 2
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_USE_USER_DATA 1

/* Fonts (firmware set; the UI mostly uses its own FontTypoder fonts). */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_FONT_PLACEHOLDER 1

/* Widgets used by main/export_path. */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_CANVAS 1
#define LV_USE_CHART 1
#define LV_USE_IMG 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_LIST 1
#define LV_USE_METER 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SPINNER 1

/* Not instantiated by any screen, but ui_helpers.c (SquareLine-generated
 * generic helpers) references dropdown/textarea/keyboard/spinbox — the
 * firmware enables all widgets, and so do we. Keyboard/textarea need
 * btnmatrix internally. */
#define LV_USE_DROPDOWN 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_SPINBOX 1
#define LV_USE_BTNMATRIX 1

/* Widgets NOT used by the UI — kept off so stray usage fails at build. */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPAN 0
#define LV_USE_SWITCH 0
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

/* sprintf */
#define LV_SPRINTF_CUSTOM 0
#define LV_SPRINTF_USE_FLOAT 0

#endif /* LV_CONF_H */
