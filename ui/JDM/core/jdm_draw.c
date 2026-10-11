// ================================================================
//  jdm_draw.c — 静态线稿实现
// ================================================================

#include "jdm_draw.h"

#include <math.h>
#include <stdlib.h>

#include "jdm_geo.h"
#include "jdm_theme.h"

static void line_delete_cb(lv_event_t *e)
{
    lv_obj_t *line = lv_event_get_current_target(e);
    lv_free(lv_obj_get_user_data(line));
    lv_obj_set_user_data(line, NULL);
}

static lv_obj_t *line_create(lv_obj_t *parent, lv_point_precise_t *pts, int n,
                             int32_t width_master, jdm_color_role_t color)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, pts, n);
    lv_obj_set_style_line_width(line, UIS(width_master), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_color(line, jdm_color(color), 0);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_add_event_cb(line, line_delete_cb, LV_EVENT_DELETE, NULL);
    return line;
}

lv_obj_t *jdm_draw_poly(lv_obj_t *parent, const jdm_pt_t *pts_in, int n,
                        int32_t width_master, jdm_color_role_t color)
{
    lv_point_precise_t *pts = lv_malloc(sizeof(lv_point_precise_t) * n);
    for (int i = 0; i < n; i++) {
        pts[i].x = UIS(pts_in[i].x);
        pts[i].y = UIS(pts_in[i].y);
    }
    return line_create(parent, pts, n, width_master, color);
}

jdm_pt_t jdm_draw_polar(int32_t mcx, int32_t mcy, int32_t r, int deg)
{
    float rad = (float)deg * 3.14159265f / 180.0f;
    jdm_pt_t p = {
        .x = mcx + (int32_t)(r * cosf(rad)),
        .y = mcy + (int32_t)(r * sinf(rad)),
    };
    return p;
}

lv_obj_t *jdm_draw_tick(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                        int32_t r1, int32_t r2, int deg,
                        int32_t width_master, jdm_color_role_t color)
{
    jdm_pt_t pts[2] = {jdm_draw_polar(mcx, mcy, r1, deg),
                       jdm_draw_polar(mcx, mcy, r2, deg)};
    return jdm_draw_poly(parent, pts, 2, width_master, color);
}
