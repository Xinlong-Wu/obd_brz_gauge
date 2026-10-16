// ================================================================
//  jdm_toggle.c — 开关实现
//  子序约定:0=中线,1=拨片,2=ON/OFF 字
// ================================================================

#include "jdm_toggle.h"

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"

lv_obj_t *jdm_toggle_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                            bool on)
{
    // 可点容器 = 轨道本体(点击区再向外扩成 88 高:建一个透明按钮挂轨道)
    lv_obj_t *hit = lv_button_create(parent);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, UIS(mx), UIS(my));
    lv_obj_set_size(hit, UIS(mw), UIS(88));

    lv_obj_t *track = lv_obj_create(hit);
    lv_obj_remove_style_all(track);
    lv_obj_set_pos(track, 0, UIS(10));
    lv_obj_set_size(track, UIS(mw), UIS(38));
    lv_obj_set_style_radius(track, UIS(4), 0);
    lv_obj_set_style_bg_color(track, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(track, jdm_color(JDM_ROLE_LINE), 0);
    lv_obj_set_style_border_width(track, UIS(2), 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 中线竖线
    lv_obj_t *mid = lv_obj_create(track);
    lv_obj_remove_style_all(mid);
    lv_obj_set_size(mid, UIS(2), UIS(24));
    lv_obj_align(mid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(mid, jdm_color(JDM_ROLE_LINE), 0);
    lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);

    // 拨片 32×28 rx2
    lv_obj_t *thumb = lv_obj_create(track);
    lv_obj_remove_style_all(thumb);
    lv_obj_set_size(thumb, UIS(32), UIS(28));
    lv_obj_set_style_radius(thumb, UIS(2), 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);

    // ON/OFF 字(y+56 = 轨道下 8)
    lv_obj_t *lbl = lv_label_create(hit);
    lv_obj_set_style_text_font(lbl, jdm_font(JDM_FONT_SMALL), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, UIS(56));

    jdm_toggle_set(hit, on);
    return hit;
}

void jdm_toggle_set(lv_obj_t *toggle, bool on)
{
    lv_obj_t *track = lv_obj_get_child(toggle, 0);
    lv_obj_t *thumb = lv_obj_get_child(track, 1);
    lv_obj_t *lbl = lv_obj_get_child(toggle, 1);

    // 拨片 x:on → w−40(贴右),off → 8(贴左);y 用渲染尺寸现算垂直居中
    // (母版坐标 ±UIS 折算在不同分辨率下会累积 ±1px,导致拨片压住轨道边)
    lv_obj_align(thumb, on ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT,
                 on ? -UIS(8) : UIS(8),
                 (lv_obj_get_height(track) - lv_obj_get_height(thumb)) / 2);
    lv_obj_set_style_bg_color(thumb, jdm_color(on ? JDM_ROLE_AMBER : JDM_ROLE_MUTED), 0);
    lv_label_set_text(lbl, on ? "ON" : "OFF");
    lv_obj_set_style_text_color(lbl, jdm_color(on ? JDM_ROLE_AMBER : JDM_ROLE_MUTED), 0);
}
