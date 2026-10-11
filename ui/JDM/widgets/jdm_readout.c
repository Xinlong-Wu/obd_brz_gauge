// ================================================================
//  jdm_readout.c — 读数卡实现
//
//  几何 = foundations.json readout 表(母版 px):
//    primary   w320  label26  value180(@y+34)   unit24(@y+232)
//    compact   w230  label22  value76 (@y+29)   unit22(@y+122)
//    eccentric w320  label26  value144(@y+66)   unit24(@y+232)
//    analog    w320  label26  value96 (@y+0)    unit24(@y+113)
// ================================================================

#include "jdm_readout.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"
#include "jdm_readout_logic.h"

typedef struct {
    uint16_t width;      // 母版
    uint16_t label_y;    // label 相对锚点偏移(母版)
    uint16_t value_y;
    uint16_t unit_y;
    jdm_font_role_t label_font;
    jdm_font_role_t value_font;
    jdm_font_role_t unit_font;
} ro_style_t;

static const ro_style_t s_style[JDM_RO_ANALOG + 1] = {
    [JDM_RO_PRIMARY]   = {320, 0,  34, 232, JDM_FONT_LABEL,   JDM_FONT_HERO,      JDM_FONT_SUPPORT},
    [JDM_RO_COMPACT]   = {230, 0,  29, 122, JDM_FONT_SMALL,   JDM_FONT_COMPACT,   JDM_FONT_SMALL},
    [JDM_RO_ECCENTRIC] = {320, 0,  66, 232, JDM_FONT_LABEL,   JDM_FONT_ECCENTRIC, JDM_FONT_SUPPORT},
    [JDM_RO_ANALOG]    = {320, 0,  0,  113, JDM_FONT_LABEL,   JDM_FONT_ANALOG,    JDM_FONT_SUPPORT},
};

void jdm_readout_create(lv_obj_t *parent, int32_t mx, int32_t my,
                        jdm_readout_size_t size, jdm_readout_t *out)
{
    const ro_style_t *st = &s_style[size];

    // 前提:parent = 全屏页容器(母版中心 (360,360))。
    // TOP_MID 的 x 偏移相对父中心,故绝对母版 x 折算为 (mx − 360)。
    int32_t ox = UIS(mx - 360);

    // label:中轴对齐,顶边在 my
    out->label = lv_label_create(parent);
    lv_obj_set_style_text_font(out->label, jdm_font(st->label_font), 0);
    lv_obj_set_style_text_color(out->label, jdm_color(JDM_ROLE_MUTED), 0);
    lv_label_set_text(out->label, "");
    lv_obj_align(out->label, LV_ALIGN_TOP_MID, ox, UIS(my));

    out->value = lv_label_create(parent);
    lv_obj_set_style_text_font(out->value, jdm_font(st->value_font), 0);
    lv_obj_set_style_text_color(out->value, jdm_color(JDM_ROLE_TEXT), 0);
    lv_label_set_text(out->value, "");
    lv_obj_align(out->value, LV_ALIGN_TOP_MID, ox, UIS(my + st->value_y));

    out->unit = lv_label_create(parent);
    lv_obj_set_style_text_font(out->unit, jdm_font(st->unit_font), 0);
    lv_obj_set_style_text_color(out->unit, jdm_color(JDM_ROLE_MUTED), 0);
    lv_label_set_text(out->unit, "");
    lv_obj_align(out->unit, LV_ALIGN_TOP_MID, ox, UIS(my + st->unit_y));
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
