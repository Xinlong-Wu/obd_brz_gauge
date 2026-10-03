#pragma once
#ifndef LV_CONF_H
#define LV_CONF_H 1
/* 单测专用 LVGL 配置 —— 从 simulator/lv_conf.h 派生,差别只在 tick:
 * 不引 SDL(LV_TICK_CUSTOM=0,内置 lv_tick),单测不需要窗口。
 * LV_COLOR_16_SWAP=1 与固件素材字节序保持一致(同 simulator/lv_conf.h)。 */

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/* Memory: plain libc malloc. */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE "stdlib.h"
#define LV_MEMCPY_MEMSET_STD 1

/* Tick: LVGL 内置计数器(单测不推进时间,仅初始化需要)。 */
#define LV_TICK_CUSTOM 0

/* Timings matching the firmware sdkconfig. */
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_DPI_DEF 130

/* Drawing */
#define LV_DRAW_COMPLEX 1
#define LV_GRADIENT_MAX_STOPS 2
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_USE_USER_DATA 1

/* Fonts (与 simulator 一致;ui_disp_item 测试用到默认字体)。 */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_FONT_FMT_TXT_LARGE 1
#define LV_USE_FONT_PLACEHOLDER 1

/* Widgets — 与 simulator/lv_conf.h 相同的开关集合。 */
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
#define LV_USE_DROPDOWN 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_SPINBOX 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_SWITCH 1

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

/* Themes(单测会注册 dummy display 并创建 label)。 */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/* Debug: asserts on, logging off. */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_ASSERT_HANDLER_INCLUDE "assert.h"
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0

/* sprintf */
#define LV_SPRINTF_CUSTOM 0
#define LV_SPRINTF_USE_FLOAT 0

#endif /* LV_CONF_H */
