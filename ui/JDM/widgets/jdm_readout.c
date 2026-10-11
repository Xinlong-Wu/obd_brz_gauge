// ================================================================
//  jdm_readout.c — 读数卡实现
//
//  几何 = foundations.json readout 表(母版 px):
//    primary   w320  label26  value180(@y+34)   unit24(@y+232)
//    compact   w230  label22  value76 (@y+29)   unit22(@y+122)
//    eccentric w320  label26  value144(@y+66)   unit24(@y+232)
//    analog    w320  label26  value96 (@y+0)    unit24(@y+113)
//  原型偏移是 hanging 语义(字形视觉顶 ≈ y+0.1em);LVGL label 的 y
//  是盒顶(字形顶还要加行内 leading,大字号达数十 px),故每行在
//  偏移上加 hang(0.1em)再减 glyph_top_off 把字形顶对到 y+hang。
// ================================================================

#include "jdm_readout.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"
#include "jdm_readout_logic.h"

typedef struct {
    uint16_t width;      // 母版
    uint16_t label_y;    // label 相对锚点偏移(母版)
    uint16_t value_y;
    uint16_t unit_y;
    uint8_t label_hang;  // 浏览器 hanging 字形下沉补偿(0.1em,母版)
    uint8_t value_hang;
    uint8_t unit_hang;
    jdm_font_role_t label_font;
    jdm_font_role_t value_font;
    jdm_font_role_t unit_font;
} ro_style_t;

static const ro_style_t s_style[JDM_RO_ANALOG + 1] = {
    [JDM_RO_PRIMARY]   = {320, 0,  34, 232, 3, 18, 2, JDM_FONT_LABEL,   JDM_FONT_HERO,      JDM_FONT_SUPPORT},
    [JDM_RO_COMPACT]   = {230, 0,  29, 122, 2,  8, 2, JDM_FONT_SMALL,   JDM_FONT_COMPACT,   JDM_FONT_SMALL},
    [JDM_RO_ECCENTRIC] = {320, 0,  66, 232, 3, 14, 2, JDM_FONT_LABEL,   JDM_FONT_ECCENTRIC, JDM_FONT_SUPPORT},
    [JDM_RO_ANALOG]    = {320, 0,  0,  113, 3, 10, 2, JDM_FONT_LABEL,   JDM_FONT_ANALOG,    JDM_FONT_SUPPORT},
};

static lv_obj_t *line_create(lv_obj_t *parent, int32_t mx, int32_t my_box,
                             jdm_font_role_t font, jdm_color_role_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, jdm_font(font), 0);
    lv_obj_set_style_text_color(l, jdm_color(color), 0);
    lv_label_set_text(l, "");
    lv_obj_align(l, LV_ALIGN_TOP_MID, UIS(mx - 360), UIS(my_box));
    return l;
}

void jdm_readout_create(lv_obj_t *parent, int32_t mx, int32_t my,
                        jdm_readout_size_t size, jdm_readout_t *out)
{
    const ro_style_t *st = &s_style[size];

    // 前提:parent = 全屏页容器(母版中心 (360,360))。

    // label:中轴对齐;三行的盒子顶 = 原型偏移 + hang − 字形顶偏移,
    // 使字形视觉顶边落在 my+偏移+hang 上(对齐原型 hanging 渲染)
    out->label = line_create(parent, mx, my + st->label_y + st->label_hang
                                 - jdm_text_glyph_top_off(jdm_font(st->label_font)),
                             st->label_font, JDM_ROLE_MUTED);

    out->value = line_create(parent, mx, my + st->value_y + st->value_hang
                                 - jdm_text_glyph_top_off(jdm_font(st->value_font)),
                             st->value_font, JDM_ROLE_TEXT);

    out->unit = line_create(parent, mx, my + st->unit_y + st->unit_hang
                                - jdm_text_glyph_top_off(jdm_font(st->unit_font)),
                            st->unit_font, JDM_ROLE_MUTED);
}

void jdm_readout_set_label(jdm_readout_t *ro, const char *text)
{
    // TOP_MID 对齐随内容尺寸变化自动保持,无需 re-align
    if (text && strcmp(lv_label_get_text(ro->label), text) != 0) {
        lv_label_set_text(ro->label, text);
    }
}

void jdm_readout_set_unit(jdm_readout_t *ro, const char *text)
{
    if (text && strcmp(lv_label_get_text(ro->unit), text) != 0) {
        lv_label_set_text(ro->unit, text);
    }
}

void jdm_readout_set_value(jdm_readout_t *ro, const char *text)
{
    if (text && strcmp(lv_label_get_text(ro->value), text) != 0) {
        lv_label_set_text(ro->value, text);
    }
}

void jdm_readout_set_channel(jdm_readout_t *ro, disp_item_t item, int32_t raw, bool valid)
{
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), item, raw, valid);
    jdm_readout_set_value(ro, buf);
}

void jdm_readout_set_warning(jdm_readout_t *ro, bool warning)
{
    lv_obj_set_style_text_color(ro->value, jdm_color(warning ? JDM_ROLE_RED : JDM_ROLE_TEXT), 0);
}
