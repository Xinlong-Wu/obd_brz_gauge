#pragma once
// ================================================================
//  jdm_fonts.h — JDM 字体入口(TinyTTF 运行时栅格化)
//
//  固件 / 模拟器 / 单测三端共用。jdm_fonts_init() 按渲染分辨率把
//  14 个角色一次性建成(jdm_fonts_logic.h 的角色表),之后 jdm_font()
//  恒返回同指针。TTF 原文嵌在 assets/fonts/(tools/gen_jdm_fonts.py 生成)。
// ================================================================

#include "lvgl.h"
#include "core/jdm_fonts_logic.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建齐角色表。必须在 lv_init() 之后、任何 UI 创建之前调用一次。
 *  @param render_res 固件传 CONFIG_OBD_UI_RENDER_RES,模拟器传 --ui-res,
 *         单测传 360。
 *  @note 字体常驻不销毁;tiny_ttf 句柄本体刻意不释放(全局持有)。 */
void jdm_fonts_init(int render_res);

/** 取角色字体(init 前调用返回 LV_FONT_DEFAULT 兜底)。 */
const lv_font_t *jdm_font(jdm_font_role_t role);

/** 预栅格化 hero 档 0-9 进字形缓存,吸收首页大字首次切开的顿挫;
 *  在 splash 显示期间后台调用一次即可。 */
void jdm_fonts_prewarm(void);

#ifdef __cplusplus
}
#endif
