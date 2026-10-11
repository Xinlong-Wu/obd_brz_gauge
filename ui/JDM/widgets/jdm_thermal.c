// ================================================================
//  jdm_thermal.c — 纵向温度柱实现
//
//  几何(母版):轨道 (142,200) 44×314;柱底 y=498,满程 288px
//  (60..120°C);刻度 y=210+k×24,长刻度 H218(每 15°C),短 H204,
//  ≥110°C 红。换算走 jdm_data_logic_thermal_permille。
// ================================================================

#include "jdm_thermal.h"

#include "core/jdm_data_logic.h"
#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_tokens.h"

// user_data:位 0 = 当前 warning 态(脏检查)
#define THERMAL_WARN(t) ((int)(intptr_t)lv_obj_get_user_data(t) & 1)

lv_obj_t *jdm_thermal_create(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, UIS(720), UIS(720));
    lv_obj_set_pos(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    // 轨道
    lv_obj_t *track = lv_obj_create(box);
    lv_obj_remove_style_all(track);
    lv_obj_set_pos(track, UIS(142), UIS(200));
    lv_obj_set_size(track, UIS(44), UIS(314));
    lv_obj_set_style_radius(track, UIS(22), 0);
    lv_obj_set_style_bg_color(track, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);

    // 动态柱(x154 w20,底边 y498)+ 顶帽(x137 w49 厚3),update 挪
    lv_obj_t *bar = lv_obj_create(box);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, UIS(20), 1);
    lv_obj_set_pos(bar, UIS(154), UIS(498));
    lv_obj_set_style_radius(bar, UIS(10), 0);
    lv_obj_set_style_bg_color(bar, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    lv_obj_t *cap = lv_obj_create(box);
    lv_obj_remove_style_all(cap);
    lv_obj_set_size(cap, UIS(49), UIS(3));
    lv_obj_set_pos(cap, UIS(137), UIS(498));
    lv_obj_set_style_bg_color(cap, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(cap, LV_OPA_COVER, 0);

    // 刻度线:13 根,y=210+k*24;major(k%3==0)H218,minor H204;≥110°C 红
    for (int k = 0; k < 13; k++) {
        int y = 210 + k * 24;
        int len = (k % 3 == 0) ? 26 : 12;   // 从 x192 起
        jdm_color_role_t role = (k <= 2) ? JDM_ROLE_RED : JDM_ROLE_LINE;
        lv_obj_t *tick = lv_obj_create(box);
        lv_obj_remove_style_all(tick);
        lv_obj_set_pos(tick, UIS(192), UIS(y));
        lv_obj_set_size(tick, UIS(len), UIS(2));
        lv_obj_set_style_bg_color(tick, jdm_color(role), 0);
        lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
    }

    // 子序约定:0=track,1=bar,2=cap,3..=ticks(update 按此取)
    return box;
}

void jdm_thermal_update(lv_obj_t *thermal, int32_t temp_c, bool valid, bool warning)
{
    lv_obj_t *bar = lv_obj_get_child(thermal, 1);
    lv_obj_t *cap = lv_obj_get_child(thermal, 2);

    if (!valid) {
        lv_obj_set_hidden(bar, true);
        lv_obj_set_hidden(cap, true);
        return;
    }
    lv_obj_set_hidden(bar, false);
    lv_obj_set_hidden(cap, false);

    // 柱顶 y = 498 − ‰×288;柱高 = 498 − y
    int permille = jdm_data_logic_thermal_permille(temp_c);
    int top = UIS(498) - UIS(288) * permille / 1000;
    int bottom = UIS(498);
    int h = bottom - top;
    if (h < UIS(20)) h = UIS(20);   // 柱高小于圆角直径时 LVGL 半径塌陷,保底
    lv_obj_set_pos(bar, UIS(154), top);
    lv_obj_set_height(bar, h);
    lv_obj_set_pos(cap, UIS(137), top - UIS(1));   // 帽中心对齐柱顶

    // 报警变色(脏检查:避免每帧重设样式)
    int w = warning ? 1 : 0;
    if (w != THERMAL_WARN(thermal)) {
        jdm_color_role_t role = warning ? JDM_ROLE_RED : JDM_ROLE_AMBER;
        lv_obj_set_style_bg_color(bar, jdm_color(role), 0);
        lv_obj_set_style_bg_color(cap, jdm_color(role), 0);
        lv_obj_set_user_data(thermal, (void *)(intptr_t)w);
    }
}
