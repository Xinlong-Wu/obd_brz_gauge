#pragma once
/* Simulator shim: ST77916.h — the real header drags in the whole QSPI/LEDC/
 * esp_lcd/CST816/TCA9554 driver stack. The compiled UI subset only uses
 * Set_Backlight() and the backlight-pin guard (ui.c). The two EXAMPLE_*
 * macros follow bsp_board.h's own override pattern so that including both
 * headers (in either order) is redefinition-free. */
#include <stdint.h>

#ifndef EXAMPLE_LCD_PIN_NUM_BK_LIGHT
#define EXAMPLE_LCD_PIN_NUM_BK_LIGHT 5
#endif

#ifndef EXAMPLE_PIN_NUM_BK_LIGHT
#define EXAMPLE_PIN_NUM_BK_LIGHT EXAMPLE_LCD_PIN_NUM_BK_LIGHT
#endif

void Set_Backlight(uint8_t Light);
