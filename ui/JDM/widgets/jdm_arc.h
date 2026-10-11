#pragma once
// ================================================================
//  jdm_arc.h — 动态值弧(components.js liveArc())
//
//  圆帽描边弧,轨道不画(轨道属 art 静态刻度)。0° = 3 点钟方向、
//  顺时针为正(SVG/LVGL 同约定)。坐标为 720 母版。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建 liveArc。圆心 (mcx,mcy) 半径 mradius,弧宽 mwidth(母版);
 *  初始 start=sweep=0。返回 arc 对象。 */
lv_obj_t *jdm_arc_create(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                         int32_t mradius, int32_t mwidth);

/** 配置角度域:start 起始角(度),sweep 总扫角;量程 0..range。 */
void jdm_arc_configure(lv_obj_t *arc, int start_deg, int sweep_deg, int range);

/** 设值(自动钳入量程);颜色取 jdm_color(role)。 */
void jdm_arc_set(lv_obj_t *arc, int value);

void jdm_arc_set_color(lv_obj_t *arc, jdm_color_role_t role);

#ifdef __cplusplus
}
#endif
