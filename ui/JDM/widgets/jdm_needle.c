// ================================================================
//  jdm_needle.c — 指针实现(line + hub,挂在一个透明容器里)
// ================================================================

#include "jdm_needle.h"

#include <math.h>
#include <stddef.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"

// 容器 user_data 布局:低 16 位 = 半径(母版),高 16 位 = 角度缓存(0.1°)
#define NEEDLE_R(g)   ((int32_t)(intptr_t)lv_obj_get_user_data(g) & 0xFFFF)
#define NEEDLE_A(g)   ((int16_t)((int32_t)(intptr_t)lv_obj_get_user_data(g) >> 16))
#define NEEDLE_PACK(r, a) ((void *)(intptr_t)((((uint32_t)(uint16_t)(a)) << 16) | ((uint32_t)(r) & 0xFFFF)))

lv_obj_t *jdm_needle_create_group(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                                  int32_t mradius, jdm_color_role_t color)
{
    // 容器 = 线的画布;线从容器中心出发,容器外框取直径
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    int32_t size = UIS(2 * mradius);
    lv_obj_set_size(box, size, size);
    lv_obj_set_pos(box, UIS(mcx) - size / 2, UIS(mcy) - size / 2);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(box, NEEDLE_PACK(mradius, 0));

    // 线铺满容器,点阵用"容器坐标"(中心在 (r,r)):lv_line 的自尺寸只
    // 取点的正向包围盒,负坐标(指向上/左的角度)会被 0×0 盒裁掉
    static lv_point_precise_t pts[2] = {{0, 0}, {0, 0}};
    lv_obj_t *line = lv_line_create(box);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_color(line, jdm_color(color), 0);
    lv_obj_set_style_line_width(line, UIS(JDM_STROKE_NEEDLE), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_size(line, UIS(2 * mradius), UIS(2 * mradius));

    lv_obj_t *hub = lv_obj_create(box);
    lv_obj_remove_style_all(hub);
    int32_t hub_d = UIS(18);   // r9 母版
    lv_obj_set_size(hub, hub_d, hub_d);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, jdm_color(color), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_center(hub);
    return box;
}

void jdm_needle_set_angle(lv_obj_t *needle_group, float angle_deg)
{
    // 脏检查精度 0.1°(int16 厘度缓存);LV_USE_FLOAT 下端点保留亚像素
    int16_t a_centi = (int16_t)lroundf(angle_deg * 10.0f);
    if (NEEDLE_A(needle_group) == a_centi) return;

    int32_t mr = NEEDLE_R(needle_group);
    float rad = angle_deg * 3.14159265f / 180.0f;
    float c_px = (float)UIS(mr);   // 容器坐标的中心 = 半径处
    static lv_point_precise_t pts[2];   // line 保存的是指针,必须静态
    pts[0].x = c_px;
    pts[0].y = c_px;
    pts[1].x = c_px + c_px * cosf(rad);
    pts[1].y = c_px + c_px * sinf(rad);

    // line 是容器的第一个子对象(hub 是第二个)
    lv_obj_t *line = lv_obj_get_child(needle_group, 0);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_user_data(needle_group, NEEDLE_PACK(mr, a_centi));
}
