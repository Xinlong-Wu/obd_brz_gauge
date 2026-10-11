// ================================================================
//  jdm_header.c — 页头实现
// ================================================================

#include "jdm_header.h"

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"

lv_obj_t *jdm_header_create(lv_obj_t *parent, const char *title, const char *sub)
{
    lv_obj_t *t = lv_label_create(parent);
    lv_obj_set_style_text_font(t, jdm_font(JDM_FONT_TITLE), 0);
    lv_obj_set_style_text_color(t, jdm_color(JDM_ROLE_AMBER), 0);
    lv_label_set_text(t, title ? title : "");
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, UIS(92));

    if (sub && sub[0]) {
        lv_obj_t *s = lv_label_create(parent);
        lv_obj_set_style_text_font(s, jdm_font(JDM_FONT_SMALL), 0);
        lv_obj_set_style_text_color(s, jdm_color(JDM_ROLE_MUTED), 0);
        lv_label_set_text(s, sub);
        lv_obj_align(s, LV_ALIGN_TOP_MID, 0, UIS(135));
    }
    return t;
}
