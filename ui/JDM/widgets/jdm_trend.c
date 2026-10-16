// ================================================================
//  jdm_trend.c — 折线 sparkline 实现
// ================================================================

#include "jdm_trend.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"

// line 的 user_data = lv_malloc 的点阵缓冲(LV_EVENT_DELETE 释放)
#define TREND_PTS(line) ((lv_point_precise_t *)lv_obj_get_user_data(line))

static void trend_delete_cb(lv_event_t *e)
{
    lv_obj_t *line = lv_event_get_current_target(e);
    lv_free(TREND_PTS(line));
    lv_obj_set_user_data(line, NULL);
}

void jdm_trend_create(lv_obj_t *parent, int32_t mx, int32_t my,
                      int32_t mw, int32_t mh, uint16_t max_n, jdm_trend_t *out)
{
    out->line = lv_line_create(parent);
    out->max_n = max_n;
    out->mx = mx; out->my = my; out->mw = mw; out->mh = mh;

    lv_point_precise_t *pts = lv_malloc(sizeof(lv_point_precise_t) * max_n);
    memset(pts, 0, sizeof(lv_point_precise_t) * max_n);
    lv_obj_set_user_data(out->line, pts);
    lv_line_set_points(out->line, pts, 0);   // 空数据不画
    lv_obj_set_style_line_width(out->line, UIS(3), 0);
    lv_obj_set_style_line_rounded(out->line, true, 0);
    lv_obj_set_style_line_color(out->line, jdm_color(JDM_ROLE_AMBER), 0);

    // 点阵缓冲与 line 同生命周期;几何以绝对坐标表达,父容器即整页
    lv_obj_set_pos(out->line, 0, 0);
    lv_obj_add_event_cb(out->line, trend_delete_cb, LV_EVENT_DELETE, NULL);
}

void jdm_trend_set(jdm_trend_t *t, const int32_t *vals, uint16_t n)
{
    if (n > t->max_n) n = t->max_n;
    lv_point_precise_t *pts = TREND_PTS(t->line);

    if (n < 2) {   // 不足两点:不画
        if (lv_line_get_point_count(t->line) != 0) {
            lv_line_set_points(t->line, pts, 0);
        }
        return;
    }

    int32_t lo = vals[0], hi = vals[0];
    for (uint16_t i = 1; i < n; i++) {
        if (vals[i] < lo) lo = vals[i];
        if (vals[i] > hi) hi = vals[i];
    }
    int32_t span = hi - lo;

    for (uint16_t i = 0; i < n; i++) {
        // 原型:x = mx + i*mw/(n-1);y = my+mh − (v−lo)/(hi−lo||1)*mh
        pts[i].x = UIS(t->mx) + (int32_t)((int64_t)i * UIS(t->mw) / (n - 1));
        int32_t h = UIS(t->mh);
        int32_t off = span ? (int32_t)((int64_t)(vals[i] - lo) * h / span) : h;   // 全等值 = 顶线
        pts[i].y = UIS(t->my) + h - off;
    }
    lv_line_set_points(t->line, pts, n);
}

void jdm_trend_set_color(jdm_trend_t *t, jdm_color_role_t role)
{
    lv_obj_set_style_line_color(t->line, jdm_color(role), 0);
}
