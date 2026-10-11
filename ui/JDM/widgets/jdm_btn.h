#pragma once
// ================================================================
//  jdm_btn.h — 动作按钮(components.js btn(),88 母版高)
//
//  8px 切角八角形(panel 底 + 2px 描边:line/amber/red 按变体)、
//  ghost 变体无底无框(原型 amber 下划线为焦点态,触摸设备不显示)、
//  按下 #26343A、禁用 40% 不透明度。label 可含 '\n' 双行——双行或
//  带箭头时用 small 字号,箭头为 back 左箭头(标签右移 14,原型
//  components.js btn():back/多行 → small + 行高 +6)。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JDM_BTN_SECONDARY = 0,   // panel 底 + line 描边
    JDM_BTN_PRIMARY,         // panel 底 + amber 描边
    JDM_BTN_GHOST,           // 无底无框,仅文字
    JDM_BTN_DANGER,          // panel 底 + red 描边
} jdm_btn_variant_t;

lv_obj_t *jdm_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                         const char *label, jdm_btn_variant_t variant);

/** 同 create,with_arrow = back 左箭头(标签右移 + small 字号)。 */
lv_obj_t *jdm_btn_create_ex(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                            const char *label, jdm_btn_variant_t variant, bool with_arrow);

/** 更新文字(支持 '\n';行数须与创建时一致)。 */
void jdm_btn_set_label(lv_obj_t *btn, const char *label);
void jdm_btn_set_disabled(lv_obj_t *btn, bool disabled);

#ifdef __cplusplus
}
#endif
