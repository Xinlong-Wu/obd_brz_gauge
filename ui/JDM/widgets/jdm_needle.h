#pragma once
// ================================================================
//  jdm_needle.h — 指针(components.js needle())
//
//  圆心 → 半径处的圆帽粗线 + 轴心圆(r9)。坐标 720 母版,角度同
//  LVGL 弧约定(0° = 3 点钟,顺时针)。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建指针(宽 = JDM_STROKE_NEEDLE 母版,amber 色)。 */
lv_obj_t *jdm_needle_create_group(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                                  int32_t mradius, jdm_color_role_t color);

/** 设角度(度)。单对象;line 与轴心作为同一容器的子级。 */
void jdm_needle_set_angle(lv_obj_t *needle_group, int angle_deg);

#ifdef __cplusplus
}
#endif
