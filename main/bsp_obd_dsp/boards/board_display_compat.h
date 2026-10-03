#pragma once
// ================================================================
//  board_display_compat.h — 屏幕符号兼容门面(M2.4)
//
//  UI 层(ui.c / ui_ext.c / screens/)历史上直接 include ST77916.h 用
//  Set_Backlight / LCD_Backlight / LCD_H_RES / LCD_V_RES。板级抽象后
//  这些符号只在 WS185 存在,统一改经本门面:
//    WS185  → 原样转发 ST77916.h
//    WS175  → LCD_H_RES/LCD_V_RES 取 board_profile();Set_Backlight/
//             LCD_Backlight 由 board_ws175_compat.c 提供并转发到面板
//             亮度命令 0x51
//  新代码请直接用 board_api.h,不要再加新符号到这里。
// ================================================================
#include "sdkconfig.h"

#if CONFIG_OBD_BOARD_WS_185

#include "bsp_obd_dsp/lcd_driver/ST77916.h"   /* EXAMPLE_LCD_* / Set_Backlight / LCD_Backlight */

/* UI 层历史符号 → WS185 驱动宏 */
#ifndef LCD_H_RES
#define LCD_H_RES EXAMPLE_LCD_WIDTH
#endif
#ifndef LCD_V_RES
#define LCD_V_RES EXAMPLE_LCD_HEIGHT
#endif

#else   /* WS175 及后续板 */

#include "bsp_obd_dsp/boards/board_api.h"

#define LCD_H_RES (board_profile()->hor_res)
#define LCD_V_RES (board_profile()->ver_res)

/* 由 board_ws175_compat.c 提供 */
extern uint8_t LCD_Backlight;
void Set_Backlight(uint8_t Light);

#endif
