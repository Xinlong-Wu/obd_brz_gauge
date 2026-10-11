// ================================================================
//  jdm_tach.c — 56 段转速弧实现
//
//  每段一个 lv_arc(bg_angles = 段弧域,indicator 恒画满),点亮/熄灭
//  只换 indicator 颜色——脏区只落在变化段。角度/点亮数换算在
//  jdm_data_logic.h(单测覆盖)。
// ================================================================

#include "jdm_tach.h"

#include "core/jdm_data_logic.h"
#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_tokens.h"

#define TACH_RADIUS 282   // 母版
#define TACH_SWEEP_END 400 // 140° + 260°

lv_obj_t *jdm_tach_create(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, UIS(720), UIS(720));
    lv_obj_set_pos(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < JDM_TACH_SEGMENTS; i++) {
        int seg_w = (i % 6 == 0) ? 18 : 10;
        int start = jdm_data_logic_tach_segment_start(i);
        int end = (i == JDM_TACH_SEGMENTS - 1) ? TACH_SWEEP_END : start + 3;
        lv_obj_t *seg = lv_arc_create(box);
        int32_t size = UIS(2 * TACH_RADIUS + seg_w);
        lv_obj_set_size(seg, size, size);
        lv_obj_align(seg, LV_ALIGN_CENTER, 0, 0);
        lv_obj_remove_style(seg, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(seg, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_width(seg, UIS(seg_w), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(seg, jdm_color(JDM_ROLE_LINE), LV_PART_INDICATOR);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_rotation(seg, 0);
        lv_arc_set_bg_angles(seg, start, end);
        lv_arc_set_range(seg, 0, 1);
        lv_arc_set_value(seg, 1);   // indicator 恒画满段域,颜色决定亮灭
    }
    return box;
}

void jdm_tach_update(lv_obj_t *tach, int32_t rpm, bool connected)
{
    int lit = connected ? jdm_data_logic_tach_lit(rpm) : 0;
    for (int i = 0; i < JDM_TACH_SEGMENTS; i++) {
        lv_obj_t *seg = lv_obj_get_child(tach, i);
        jdm_color_role_t want;
        if (i < lit) {
            want = (i >= JDM_TACH_RED_AT) ? JDM_ROLE_RED
                 : (i >= JDM_TACH_AMBER_AT) ? JDM_ROLE_AMBER : JDM_ROLE_TEXT;
        } else {
            want = JDM_ROLE_LINE;
        }
        lv_color_t next = jdm_color(want);
        if (!lv_color_eq(lv_obj_get_style_arc_color(seg, LV_PART_INDICATOR), next)) {
            lv_obj_set_style_arc_color(seg, next, LV_PART_INDICATOR);
        }
    }
}
