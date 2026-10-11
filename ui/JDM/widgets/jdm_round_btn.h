#pragma once
// ================================================================
//  jdm_round_btn.h — 圆形 ± 按钮(components.js roundButton())
//  圆 r43 panel 底 line 描边 2,± 符号线宽 3;点击区即圆(88 母版)。
//  (mx,my) 为原型的锚点;圆心 = (mx + mw/2, my + 44)。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** label: "+" 或 "-"(决定符号)。 */
lv_obj_t *jdm_round_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                               const char *label);

void jdm_round_btn_set_disabled(lv_obj_t *btn, bool disabled);

#ifdef __cplusplus
}
#endif
