/* ui_fonts.h — FontTypoder 八档字体的 TinyTTF 运行时渲染入口
 *
 * 固件 / 模拟器 / 单测三端共用。八档全局 lv_font_t 符号(ui.h 里 extern)
 * 在 ui_fonts_init() 时填充,调用点(&ui_font_X)与旧位图方案完全一致。
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 创建八档字体。必须在 lv_init() 之后、任何 UI 创建之前调用一次。
 *  @param render_res 渲染分辨率:固件传 CONFIG_OBD_UI_RENDER_RES,
 *         模拟器传运行时 s_ui_res(--ui-res),单测传 360。
 *  @note 字体常驻不销毁;创建出的 tiny_ttf 句柄本体刻意不释放
 *        (全局变量持有其字段副本,user_data 指向的上下文必须存活)。 */
void ui_fonts_init(int render_res);

/** 预栅格化 Size140 的 0-9/N/R 进字形缓存,吸收齿轮页首次切大字的
 *  一次性栅格化顿挫;在 Logo 屏显示期间后台调用一次即可。 */
void ui_fonts_prewarm_size140(void);

#ifdef __cplusplus
}
#endif
