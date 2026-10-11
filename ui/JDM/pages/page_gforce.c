// ================================================================
//  page_gforce.c — G 力(原型 10 页)
//
//  1g/2g 同心圆 + 十字与端帽刻度 + 当前点;±1.5g 数据映射到
//  2g 圆内(radius 238 的 1/2 = 119 对应 1g)。提醒阈值圈虚线。
// ================================================================

#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"

#include <stdio.h>

#define GF_RADIUS 238
#define GF_1G (GF_RADIUS / 2)   // 119

static lv_obj_t *s_ring1;
static lv_obj_t *s_ring2;
static lv_obj_t *s_point_ring;
static lv_obj_t *s_point;

static lv_obj_t *ring_create(lv_obj_t *parent, int32_t mr, int32_t bw, jdm_color_role_t c)
{
    lv_obj_t *ring = lv_obj_create(parent);
    lv_obj_remove_style_all(ring);
    int32_t d = UIS(2 * mr);
    lv_obj_set_size(ring, d, d);
    lv_obj_set_pos(ring, UIS(360) - d / 2, UIS(360) - d / 2);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(ring, jdm_color(c), 0);
    lv_obj_set_style_border_width(ring, UIS(bw), 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return ring;
}

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "G-FORCE", NULL);

    s_ring2 = ring_create(page, GF_RADIUS, 3, JDM_ROLE_AMBER);
    s_ring1 = ring_create(page, GF_1G, 2, JDM_ROLE_LINE);

    // 十字主线 + amber 端帽 + 1g 内刻度
    static const jdm_pt_t cross[] = {{122, 360}, {598, 360}};
    jdm_draw_poly(page, cross, 2, 2, JDM_ROLE_LINE);
    static const jdm_pt_t crossv[] = {{360, 122}, {360, 598}};
    jdm_draw_poly(page, crossv, 2, 2, JDM_ROLE_LINE);
    static const struct { jdm_pt_t a, b; } caps[4] = {
        {{122, 360}, {152, 360}}, {{568, 360}, {598, 360}},
        {{360, 122}, {360, 152}}, {{360, 568}, {360, 598}},
    };
    for (int i = 0; i < 4; i++) {
        jdm_pt_t s[2] = {caps[i].a, caps[i].b};
        jdm_draw_poly(page, s, 2, 6, JDM_ROLE_AMBER);
    }
    static const struct { jdm_pt_t a, b; } ticks[4] = {
        {{241, 360}, {258, 360}}, {{462, 360}, {479, 360}},
        {{360, 241}, {360, 258}}, {{360, 462}, {360, 479}},
    };
    for (int i = 0; i < 4; i++) {
        jdm_pt_t s[2] = {ticks[i].a, ticks[i].b};
        jdm_draw_poly(page, s, 2, 4, JDM_ROLE_AMBER);
    }
    // 8 个刻度标签
    static const struct { int32_t x, y; const char *txt; } lbls[8] = {
        {385, 143, "+2"}, {382, 251, "+1"}, {382, 445, "\xE2\x88\x92" "1"}, {385, 555, "\xE2\x88\x92" "2"},
        {146, 378, "\xE2\x88\x92" "2"}, {261, 378, "\xE2\x88\x92" "1"}, {459, 378, "+1"}, {576, 378, "+2"},
    };
    for (int i = 0; i < 8; i++) {
        jdm_text(page, lbls[i].x, lbls[i].y, lbls[i].txt, JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    // 当前点:外圈描边圆 + 内点
    s_point_ring = ring_create(page, 16, 2, JDM_ROLE_AMBER);
    s_point = lv_obj_create(page);
    lv_obj_remove_style_all(s_point);
    lv_obj_set_size(s_point, UIS(20), UIS(20));
    lv_obj_set_style_radius(s_point, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_point, jdm_color(JDM_ROLE_TEXT), 0);
    lv_obj_set_style_bg_opa(s_point, LV_OPA_COVER, 0);
}

static void update(void)
{
    int32_t lat = 0, lon = 0;
    bool ok = jdm_data_read_cache(DISP_ITEM_GFORCE_LAT, &lat) &&
              jdm_data_read_cache(DISP_ITEM_GFORCE_LON, &lon) &&
              jdm_conn_active();
    if (!ok) {
        lv_obj_set_hidden(s_point, true);
        lv_obj_set_hidden(s_point_ring, true);
        return;
    }
    lv_obj_set_hidden(s_point, false);
    lv_obj_set_hidden(s_point_ring, false);
    // x = 360 + lat(g)×119, y = 360 − lon(g)×119;raw 0.01g
    int32_t px = UIS(360 + lat * GF_1G / 100) - UIS(10);
    int32_t py = UIS(360 - lon * GF_1G / 100) - UIS(10);
    lv_obj_set_pos(s_point, px, py);
    lv_obj_set_pos(s_point_ring, UIS(360 + lat * GF_1G / 100) - UIS(16), UIS(360 - lon * GF_1G / 100) - UIS(16));
}

const jdm_page_t page_gforce = {"gforce", create, update, 16};
