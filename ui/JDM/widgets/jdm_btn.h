#pragma once
// ================================================================
//  jdm_btn.h — 动作按钮(components.js actionButton(),88 母版高)
//
//  原型是 8px 切角矩形;LVGL 无切角,近似为 r4 圆角(已知的视觉偏差,
//  其余规格忠实:panel 底 + 2px 描边(line/amber/red 按变体)、ghost
//  变体仅底部 amber 下划线、按下 #26343A、禁用 40% 不透明度)。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JDM_BTN_SECONDARY = 0,   // panel 底 + line 描边
    JDM_BTN_PRIMARY,         // panel 底 + amber 描边
    JDM_BTN_GHOST,           // 无底无框,底部 amber 下划线
    JDM_BTN_DANGER,          // panel 底 + red 描边
} jdm_btn_variant_t;

lv_obj_t *jdm_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                         const char *label, jdm_btn_variant_t variant);

void jdm_btn_set_label(lv_obj_t *btn, const char *label);
void jdm_btn_set_disabled(lv_obj_t *btn, bool disabled);

#ifdef __cplusplus
}
#endif
