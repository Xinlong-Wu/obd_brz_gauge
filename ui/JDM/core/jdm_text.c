// ================================================================
//  jdm_text.c — 文本基元实现
// ================================================================

#include "jdm_text.h"

#include "jdm_geo.h"
#include "jdm_theme.h"

lv_obj_t *jdm_text(lv_obj_t *parent, int32_t mx, int32_t my, const char *text,
                   jdm_font_role_t font, jdm_color_role_t color,
                   jdm_text_anchor_t anchor)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, jdm_font(font), 0);
    lv_obj_set_style_text_color(l, jdm_color(color), 0);
    lv_label_set_text(l, text ? text : "");

    switch (anchor) {
    case JDM_TEXT_START:
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, UIS(mx), UIS(my));
        break;
    case JDM_TEXT_END:
        lv_obj_align(l, LV_ALIGN_TOP_RIGHT, UIS(mx - 720), UIS(my));
        break;
    case JDM_TEXT_MIDDLE:
    default:
        lv_obj_align(l, LV_ALIGN_TOP_MID, UIS(mx - 360), UIS(my));
        break;
    }
    return l;
}
