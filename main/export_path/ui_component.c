// ================================================================
//  ui_component.c — 内置仪表组件实现(M3.2)
//
//  渲染策略:小组件(≤160px)单字号,大组件双字号(名称/单位小、数值大);
//  所有颜色走 ui_theme_color_lv(换肤零改动);数值格式复用
//  disp_item 的除数约定(raw = natural × div)。
// ================================================================

#include "ui_component.h"
#include "ui_theme.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <string.h>
#include "ui_res.h"

/* 组件私有数据(obj->user_data 指向,随 obj 生命周期)。 */
typedef struct {
    ui_comp_desc_t desc;
    lv_obj_t *main;     // 数值 label / 弧 / 条
    lv_obj_t *name;     // 名称 label(可空)
    lv_obj_t *unit;     // 单位 label(可空)
    int32_t   shown;    // 上次显示值(脏检查)
    bool      shown_valid;
} ui_comp_priv_t;

static const char *const s_comp_names[UI_COMP_TYPE_COUNT] = {
    [UI_COMP_VALUE]    = "value",
    [UI_COMP_ARC]      = "arc",
    [UI_COMP_BAR]      = "bar",
    [UI_COMP_BIG_NUM]  = "bignum",
    [UI_COMP_GFORCE]   = "gforce",
};

int ui_comp_type_from_name(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < (int)UI_COMP_TYPE_COUNT; i++) {
        if (strcmp(name, s_comp_names[i]) == 0) return i;
    }
    return -1;
}

const char *ui_comp_type_name(ui_comp_type_t type)
{
    if ((int)type < 0 || (int)type >= (int)UI_COMP_TYPE_COUNT) return "";
    return s_comp_names[type];
}

const ui_comp_desc_t *ui_comp_desc_of(lv_obj_t *comp)
{
    if (!comp) return NULL;
    ui_comp_priv_t *p = (ui_comp_priv_t *)lv_obj_get_user_data(comp);
    return p ? &p->desc : NULL;
}

/** 数值文本:按通道除数规则格式化(同 ui_disp_item 的约定)。 */
static void comp_format(char *buf, size_t len, disp_item_t ch, int32_t raw)
{
    const needle_scale_meta_t *ns = ui_disp_item_scale(ch);
    if (ns->div > 1) {
        int32_t a = raw < 0 ? -raw : raw;
        snprintf(buf, len, "%s%d.%d", raw < 0 ? "-" : "",
                 (int)(a / ns->div), (int)(a % ns->div));
    } else {
        snprintf(buf, len, "%d", (int)raw);
    }
}

/** 小号样式字体的选择(按槽宽粗分档,避免为组件引入新字体资产)。 */
static const lv_font_t *comp_value_font(int16_t w)
{
    if (w >= 200) return &ui_font_FontTypoderSize56;
    if (w >= 120) return &ui_font_FontTypoderSize44;
    return &ui_font_FontTypoderSize24;
}

static lv_obj_t *comp_label(lv_obj_t *parent, const lv_font_t *font,
                            lv_color_t color, bool center)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    if (center) lv_obj_center(l);
    return l;
}

lv_obj_t *ui_comp_create(const ui_comp_desc_t *desc, lv_obj_t *parent)
{
    if (!desc || !parent) return NULL;
    if ((int)desc->type < 0 || (int)desc->type >= (int)UI_COMP_TYPE_COUNT) return NULL;
    if ((int)desc->channel < 0 || (int)desc->channel >= (int)DISP_ITEM_COUNT) return NULL;
    if (desc->w <= 0 || desc->h <= 0) return NULL;

    ui_comp_priv_t *p = lv_malloc(sizeof(ui_comp_priv_t));
    if (!p) return NULL;
    memset(p, 0, sizeof(*p));
    p->desc = *desc;

    lv_obj_t *comp = lv_obj_create(parent);
    lv_obj_set_pos(comp, desc->x, desc->y);
    lv_obj_set_size(comp, desc->w, desc->h);
    lv_obj_clear_flag(comp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(comp, 0, 0);
    lv_obj_set_style_border_width(comp, 0, 0);
    lv_obj_set_style_bg_opa(comp, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(comp, p);

    char nbuf[8];
    switch (desc->type) {
    case UI_COMP_VALUE: {
        lv_obj_t *name = comp_label(comp, &ui_font_FontTypoderSize16,
                                    ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), false);
        lv_label_set_text(name, ui_disp_item_name((uint8_t)desc->channel));
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 0);

        p->main = comp_label(comp, comp_value_font(desc->w),
                             ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), true);

        lv_obj_t *unit = comp_label(comp, &ui_font_FontTypoderSize16,
                                    ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), false);
        lv_label_set_text(unit, ui_disp_item_unit((uint8_t)desc->channel));
        lv_obj_align(unit, LV_ALIGN_BOTTOM_MID, 0, 0);
        (void)nbuf;
        break;
    }
    case UI_COMP_ARC: {
        lv_obj_t *arc = lv_arc_create(comp);
        lv_obj_set_size(arc, desc->w < desc->h ? desc->w : desc->h, desc->w < desc->h ? desc->w : desc->h);
        lv_obj_center(arc);
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_range(arc, 0, 100);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(arc, ui_theme_color_lv(UI_COLOR_ARC_TRACK), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);
        p->main = comp_label(comp, comp_value_font(desc->w * 3 / 5),
                             ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), true);
        break;
    }
    case UI_COMP_BAR: {
        lv_obj_t *bar = lv_bar_create(comp);
        lv_obj_set_size(bar, desc->w, desc->h / 3);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_style_radius(bar, UIS(6), LV_PART_MAIN);
        lv_obj_set_style_radius(bar, UIS(6), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(bar, ui_theme_color_lv(UI_COLOR_ARC_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);
        p->main = bar;
        break;
    }
    case UI_COMP_BIG_NUM: {
        p->main = comp_label(comp, comp_value_font(desc->w),
                             ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), true);
        break;
    }
    case UI_COMP_GFORCE: {
        // 圆形底 + 十字线 + 中央点(轨迹/历史随 M4 布局细化)
        lv_obj_t *c = lv_obj_create(comp);
        lv_obj_set_size(c, desc->w < desc->h ? desc->w : desc->h, desc->w < desc->h ? desc->w : desc->h);
        lv_obj_center(c);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(c, ui_theme_color_lv(UI_COLOR_ARC_TRACK), 0);
        lv_obj_set_style_border_width(c, UIS(4), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        p->main = lv_obj_create(c);   // 活动点,位置由 update 挪
        lv_obj_set_size(p->main, UIS(20), UIS(20));
        lv_obj_set_style_radius(p->main, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p->main, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), 0);
        lv_obj_set_style_bg_opa(p->main, 255, 0);
        break;
    }
    default:
        lv_obj_del(comp);
        lv_free(p);
        return NULL;
    }
    return comp;
}

bool ui_comp_update(lv_obj_t *comp)
{
    if (!comp) return false;
    ui_comp_priv_t *p = (ui_comp_priv_t *)lv_obj_get_user_data(comp);
    if (!p) return false;

    const needle_scale_meta_t *ns = ui_disp_item_scale(p->desc.channel);
    char buf[16];

    if (p->desc.type == UI_COMP_GFORCE) {
        int32_t lat = 0, lon = 0;
        bool ok = ui_disp_item_read_cache(DISP_ITEM_GFORCE_LAT, &lat) &&
                  ui_disp_item_read_cache(DISP_ITEM_GFORCE_LON, &lon);
        if (!ok) return false;
        // ±1.5g 映射到 ±(圆半径-点径);点挂在圆容器上,以圆心对齐偏移
        lv_obj_t *circle = lv_obj_get_parent(p->main);
        lv_coord_t r = lv_obj_get_width(circle) / 2;
        int32_t px = (int32_t)((double)lon / 150.0 * (r - 6));
        int32_t py = (int32_t)((double)lat / 150.0 * (r - 6));
        lv_obj_align_to(p->main, circle, LV_ALIGN_CENTER,
                        (lv_coord_t)px, (lv_coord_t)-py);
        return true;
    }

    int32_t raw = 0;
    if (!ui_disp_item_read_cache(p->desc.channel, &raw)) {
        // 无效:刷 "--"(含首个样本即无效的情形,占位文本不能残留)。
        // set_text 内部有变更检测,重复调用无额外代价。
        p->shown_valid = false;
        if (p->desc.type != UI_COMP_BAR &&
            p->desc.type != UI_COMP_GFORCE &&
            p->main && lv_obj_check_type(p->main, &lv_label_class)) {
            lv_label_set_text(p->main, "--");
        }
        return false;
    }
    if (p->shown_valid && raw == p->shown) return true;   // 脏检查
    p->shown = raw;
    p->shown_valid = true;   // 写入后才算有效(首写 0 值也必须渲染)

    int32_t pct = 0;
    int32_t span = ns->nmax - ns->nmin;
    if (span > 0) pct = ((raw / ns->div) - ns->nmin) * 100 / span;

    if (p->desc.type == UI_COMP_VALUE || p->desc.type == UI_COMP_BIG_NUM) {
        comp_format(buf, sizeof(buf), p->desc.channel, raw);
        lv_label_set_text(p->main, buf);
    } else if (p->desc.type == UI_COMP_ARC) {
        // main 是数值 label,弧是它的前一个兄弟
        lv_obj_t *arc = lv_obj_get_child(lv_obj_get_parent(p->main), 0);
        lv_arc_set_value(arc, (int16_t)(pct < 0 ? 0 : (pct > 100 ? 100 : pct)));
        comp_format(buf, sizeof(buf), p->desc.channel, raw);
        lv_label_set_text(p->main, buf);
    } else if (p->desc.type == UI_COMP_BAR) {
        lv_bar_set_value(p->main, (int16_t)(pct < 0 ? 0 : (pct > 100 ? 100 : pct)), LV_ANIM_OFF);
    }
    return true;
}
