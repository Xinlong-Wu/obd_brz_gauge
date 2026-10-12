#pragma once
// ================================================================
//  jdm_data_logic.h — 数据映射纯逻辑(static inline,零 LVGL 依赖)
//
//  原型语义:值 → 位置即时映射,无补间。这里集中"raw → 渲染量"的
//  全部换算(百分比、270° 弧终点角、56 段转速点亮数、温度柱高),
//  tests/ 直接编译断言(见 test_jdm_data.c)。
// ================================================================

#include <stdint.h>
#include <stdbool.h>

#include "core/jdm_data.h"

/** raw → 自然单位量程内的百分比 0..100(带 div 折算,钳出界)。 */
static inline int jdm_data_logic_pct(int32_t raw, const jdm_data_range_t *r)
{
    if (!r) return 0;
    int32_t span = r->nmax - r->nmin;
    if (span <= 0) return 0;
    int32_t pct = ((raw / r->div) - r->nmin) * 100 / span;
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return (int)pct;
}

/** 原型 270° 表盘:起点恒 135°(LVGL 角度约定,0° = 3 点钟方向,
 *  顺时针),pct(0..100)→ 终点角 135..405。 */
static inline int jdm_data_logic_arc270_end(int pct)
{
    return 135 + pct * 270 / 100;
}

// ---- 56 段转速弧(page_gear / tachometer)----
// 段数是 UI 几何常量;量程/警戒区起点是**车型配置**(tach_cfg_t,见
// vehicle_profiles.h),以参数贯穿下列纯逻辑函数。

#define JDM_TACH_SEGMENTS   56

/** 点亮段数 = ceil(clamp(rpm)/rpm_max × 56),rpm>0 保底 1 段显示"活着"。 */
static inline int jdm_data_logic_tach_lit(int32_t rpm, int32_t rpm_max)
{
    if (rpm < 0) rpm = 0;
    if (rpm > rpm_max) rpm = rpm_max;
    return (int)((rpm * JDM_TACH_SEGMENTS + rpm_max - 1) / rpm_max);
}

/** 段角度几何:56 段扫 260°,起始 140°;返回第 i 段的起始角(浮点度,
 *  精确 4.6429° 节距——LV_USE_FLOAT 下 lv_arc 角度为浮点);段末角恒 400°。 */
static inline float jdm_data_logic_tach_segment_start(int i)
{
    return 140.0f + (float)i * 260.0f / (float)JDM_TACH_SEGMENTS;
}

/** 第 i 段起始位置对应的转速(段色带判定用)。 */
static inline int32_t jdm_data_logic_tach_seg_rpm(int i, int32_t rpm_max)
{
    return i * rpm_max / JDM_TACH_SEGMENTS;
}

/** 温度柱:返回柱内归一化高度 0..1000(‰),钳出界;量程 [lo,hi] 为参数。 */
static inline int jdm_data_logic_thermal_permille(int32_t temp, int32_t lo, int32_t hi)
{
    if (temp <= lo) return 0;
    if (temp >= hi) return 1000;
    return (int)((temp - lo) * 1000 / (hi - lo));
}
