// ================================================================
//  jdm_chip.c — 状态小条实现
// ================================================================

#include "jdm_chip.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"

static lv_obj_t *build(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, UIS(mx), UIS(my));
    lv_obj_set_size(c, UIS(mw), UIS(44));
    lv_obj_set_style_radius(c, UIS(8), 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *l = lv_label_create(c);
    lv_obj_set_style_text_font(l, jdm_font(JDM_FONT_SMALL), 0);
    lv_obj_center(l);
    return c;
}

lv_obj_t *jdm_chip_create(lv_obj_t *parent, int32_t mx, int32_t my,
                          int32_t mw, const char *text, jdm_color_role_t bg)
{
    lv_obj_t *c = build(parent, mx, my, mw);
    lv_obj_set_style_bg_color(c, jdm_color(bg), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_t *l = lv_obj_get_child(c, 0);
    bool dark_ink = (bg == JDM_ROLE_AMBER || bg == JDM_ROLE_RED);
    lv_obj_set_style_text_color(l, jdm_color(dark_ink ? JDM_ROLE_FACE : JDM_ROLE_TEXT), 0);
    lv_label_set_text(l, text ? text : "");
    return c;
}

void jdm_chip_set(lv_obj_t *chip, const char *text, jdm_color_role_t bg)
{
    lv_obj_set_style_bg_color(chip, jdm_color(bg), 0);
    lv_obj_t *l = lv_obj_get_child(chip, 0);
    bool dark_ink = (bg == JDM_ROLE_AMBER || bg == JDM_ROLE_RED);
    lv_obj_set_style_text_color(l, jdm_color(dark_ink ? JDM_ROLE_FACE : JDM_ROLE_TEXT), 0);
    if (strcmp(lv_label_get_text(l), text ? text : "") != 0) {
        lv_label_set_text(l, text ? text : "");
    }
}
