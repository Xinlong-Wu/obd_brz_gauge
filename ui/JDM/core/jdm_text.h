#pragma once
// ================================================================
//  jdm_text.h — 文本基元(原型 components.js t() 的 LVGL 对应)
//
//  (mx,my) = 720 母版绝对坐标,y 是文字顶边(SVG hanging 语义);
//  anchor:middle = 水平居中于 mx,start = 左缘,end = 右缘。
//  前提:parent 为全屏页容器。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"
#include "core/jdm_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JDM_TEXT_MIDDLE = 0,
    JDM_TEXT_START,
    JDM_TEXT_END,
} jdm_text_anchor_t;

/** 建静态文本(内容建后不改;要改的用 label 返回值自行 set_text)。 */
lv_obj_t *jdm_text(lv_obj_t *parent, int32_t mx, int32_t my, const char *text,
                   jdm_font_role_t font, jdm_color_role_t color,
                   jdm_text_anchor_t anchor);

#ifdef __cplusplus
}
#endif
