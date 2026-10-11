#pragma once
// ================================================================
//  jdm_draw.h — 静态线稿绘图助手(表盘 art 刻度/分隔线/线稿)
//
//  点阵在创建时按 UIS() 折算并 lv_malloc,line 删除时自动释放;
//  坐标为 720 母版绝对值。用于"建一次不再动"的静态元素——
//  逐帧变动的线(路景等)请自行持有点阵。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int32_t x, y; } jdm_pt_t;

/** 折线(圆帽)。pts 母版坐标,创建时拷贝。 */
lv_obj_t *jdm_draw_poly(lv_obj_t *parent, const jdm_pt_t *pts, int n,
                        int32_t width_master, jdm_color_role_t color);

/** 极坐标刻度线:圆 (mcx,mcy) 从 r1 到 r2,角度 deg(度,0°=3 点钟顺时针)。 */
lv_obj_t *jdm_draw_tick(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                        int32_t r1, int32_t r2, int deg,
                        int32_t width_master, jdm_color_role_t color);

/** 极坐标点(母版)。 */
jdm_pt_t jdm_draw_polar(int32_t mcx, int32_t mcy, int32_t r, int deg);

#ifdef __cplusplus
}
#endif
