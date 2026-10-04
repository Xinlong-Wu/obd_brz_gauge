// 非 WS185 板兼容壳:提供 UI 层历史使用的 ST77916 背光符号,转发到板级亮度。
// WS175 / WS128 等后续板构建时编译(见 board_display_compat.h 门面)。
#include "sdkconfig.h"

#if !CONFIG_OBD_BOARD_WS_185

#include "bsp_obd_dsp/boards/board_display_compat.h"

uint8_t LCD_Backlight = 0;

void Set_Backlight(uint8_t Light)
{
    LCD_Backlight = Light;
    (void)board_set_brightness(Light);
}

#endif /* !CONFIG_OBD_BOARD_WS_185 */
