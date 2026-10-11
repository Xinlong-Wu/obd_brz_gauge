// ================================================================
//  jdm_text.c — 文本基元实现
// ================================================================

#include "jdm_text.h"

#include "jdm_geo.h"
#include "jdm_theme.h"

int32_t jdm_text_glyph_top_off(const lv_font_t *f)
{
    // LVGL label 盒顶到字形视觉顶的偏移:行内基线在 line_height−base_line,
    // 字形位图顶再抬高 box_h+ofs_y(ofs_y 向上为正)。用数字 '0' 度量——
    // 本 UI 全为大写字母与数字,帽线一致。
    // 字体按渲染分辨率栅格化,度量是 res 像素;这里换算回母版像素,
    // 供母版坐标定位使用。
    lv_font_glyph_dsc_t d;
    if (!f || !lv_font_get_glyph_dsc(f, &d, '0', 0)) return 0;
    int32_t off = (f->line_height - f->base_line) - ((int32_t)d.box_h + d.ofs_y);
    if (off < 0) off = 0;
    return off * JDM_GEO_MASTER / UI_RENDER_RES;
}

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
