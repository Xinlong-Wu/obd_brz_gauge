// ================================================================
//  jdm_arc.c — 动态值弧实现(lv_arc 裸用法:轨道透明,只画 indicator)
// ================================================================

#include "jdm_arc.h"

#include <stdint.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"

lv_obj_t *jdm_arc_create(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                         int32_t mradius, int32_t mwidth)
{
    lv_obj_t *arc = lv_arc_create(parent);
    // 对象外框 = 弧半径 + 半宽(圆帽),pad 全零,圆心定位于 (mcx,mcy)
    int32_t size = UIS(2 * mradius + mwidth);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_pos(arc, UIS(mcx) - size / 2, UIS(mcy) - size / 2);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);        // 不画旋钮
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);    // 轨道不画(宽度 0 最稳)
    lv_obj_set_style_arc_color(arc, jdm_color(JDM_ROLE_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, UIS(mwidth), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(arc, 0);                         // 0° = 3 点钟,顺时针
    lv_arc_set_bg_angles(arc, 0, 360);                   // 由 configure 收窄
    lv_obj_set_user_data(arc, NULL);                     // 量程由 configure 记录
    return arc;
}

void jdm_arc_configure(lv_obj_t *arc, int start_deg, int sweep_deg, int range)
{
    if (sweep_deg <= 0) sweep_deg = 1;
    if (range <= 0) range = 1;
    lv_arc_set_bg_angles(arc, start_deg, start_deg + sweep_deg);
    // 值域 = 数据域(非度域):LV_USE_FLOAT 下 LVGL 把 value→角度
    // 做浮点映射,每个数据步进都有精确角度,无整度量化
    lv_arc_set_range(arc, 0, range);
    lv_arc_set_value(arc, 0);
    lv_obj_set_user_data(arc, (void *)(intptr_t)range);   // 记住量程供 set 用
}

void jdm_arc_set(lv_obj_t *arc, int value)
{
    int range = (int)(intptr_t)lv_obj_get_user_data(arc);
    if (range <= 0) range = 1;
    if (value < 0) value = 0;
    if (value > range) value = range;
    lv_arc_set_value(arc, value);
}

void jdm_arc_set_color(lv_obj_t *arc, jdm_color_role_t role)
{
    lv_obj_set_style_arc_color(arc, jdm_color(role), LV_PART_INDICATOR);
}
