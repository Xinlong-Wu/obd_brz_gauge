// ================================================================
//  jdm_btn.c — 动作按钮实现
// ================================================================

#include "jdm_btn.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"
#include "core/jdm_tokens.h"

// 按下填充色(components.css --control-pressed #26343a)
#define JDM_HEX_PRESSED 0x26343A

lv_obj_t *jdm_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                         const char *label, jdm_btn_variant_t variant)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, UIS(mx), UIS(my));
    lv_obj_set_size(b, UIS(mw), UIS(JDM_CTL_ACTION_H));

    bool ghost = (variant == JDM_BTN_GHOST);
    if (ghost) {
        // ghost:无底无框,底部 amber 下划线(y+76 母版 ≈ 底边)
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(b, UIS(2), 0);
        lv_obj_set_style_border_side(b, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(b, jdm_color(JDM_ROLE_AMBER), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(JDM_HEX_PRESSED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_radius(b, UIS(4), 0);
        lv_obj_set_style_bg_color(b, jdm_color(JDM_ROLE_PANEL), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        jdm_color_role_t border = (variant == JDM_BTN_DANGER) ? JDM_ROLE_RED
                                : (variant == JDM_BTN_PRIMARY) ? JDM_ROLE_AMBER
                                : JDM_ROLE_LINE;
        lv_obj_set_style_border_color(b, jdm_color(border), 0);
        lv_obj_set_style_border_width(b, UIS(2), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(JDM_HEX_PRESSED), LV_STATE_PRESSED);
    }

    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, jdm_font(JDM_FONT_LABEL), 0);
    lv_obj_set_style_text_color(l, jdm_color(JDM_ROLE_TEXT), 0);
    lv_label_set_text(l, label ? label : "");
    lv_obj_center(l);
    return b;
}

void jdm_btn_set_label(lv_obj_t *btn, const char *label)
{
    lv_obj_t *l = lv_obj_get_child(btn, 0);
    if (strcmp(lv_label_get_text(l), label ? label : "") != 0) {
        lv_label_set_text(l, label ? label : "");
    }
}

void jdm_btn_set_disabled(lv_obj_t *btn, bool disabled)
{
    lv_obj_set_style_opa(btn, disabled ? JDM_CTL_DISABLED_OPA : LV_OPA_COVER, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    if (!disabled) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
}
