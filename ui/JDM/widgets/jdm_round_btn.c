// ================================================================
//  jdm_round_btn.c — 圆形 ± 按钮实现
//
//  符号用文本 "+" / "−"(Plex Mono 500)近似原型的 3px 线稿——
//  宽度/间距一致,省一组 line 对象。
// ================================================================

#include "jdm_round_btn.h"

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"
#include "core/jdm_tokens.h"

// 按下填充色(components.css --control-pressed)
#define JDM_HEX_PRESSED 0x26343A

lv_obj_t *jdm_round_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                               const char *label)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_remove_style_all(b);
    int32_t d = UIS(2 * JDM_CTL_ROUND_RADIUS);   // 直径 86
    lv_obj_set_size(b, d, d);
    // 圆心 (mx+mw/2, my+44):左上角坐标须在母版空间一次折算——
    // "UIS(圆心) − d/2" 两段各自取整,240 档下累计 +1px 右/下偏移
    lv_obj_set_pos(b, UIS(mx + mw / 2 - JDM_CTL_ROUND_RADIUS),
                      UIS(my + 44 - JDM_CTL_ROUND_RADIUS));
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, jdm_color(JDM_ROLE_LINE), 0);
    lv_obj_set_style_border_width(b, UIS(2), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(JDM_HEX_PRESSED), LV_STATE_PRESSED);

    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, jdm_font(JDM_FONT_TITLE), 0);
    lv_obj_set_style_text_color(l, jdm_color(JDM_ROLE_TEXT), 0);
    lv_label_set_text(l, label ? label : "+");
    lv_obj_center(l);
    return b;
}

void jdm_round_btn_set_disabled(lv_obj_t *btn, bool disabled)
{
    lv_obj_set_style_opa(btn, disabled ? JDM_CTL_DISABLED_OPA : LV_OPA_COVER, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    if (!disabled) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
}
