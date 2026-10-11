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
// 量程 / 段数 / 警戒区以转速值定义(改这里即可重排表盘):
//   琥珀(警告)从 AMBER_RPM 起,红(报警)从 RED_RPM 起到满量程;
//   RED_RPM 须与 jdm_alerts_logic.h 的 JDM_ALERTS_RPM_LIMIT 一致
//   (进红区即触发 critical 报警)。
#define JDM_TACH_SEGMENTS   56
#define JDM_TACH_RPM_MAX    8000
#define JDM_TACH_AMBER_RPM  5700
#define JDM_TACH_RED_RPM    7000

/** 点亮段数 = ceil(clamp(rpm)/8000 × 56),rpm<8000 保底 1 段显示"活着"。 */
static inline int jdm_data_logic_tach_lit(int32_t rpm)
{
    if (rpm < 0) rpm = 0;
    if (rpm > JDM_TACH_RPM_MAX) rpm = JDM_TACH_RPM_MAX;
    return (int)((rpm * JDM_TACH_SEGMENTS + JDM_TACH_RPM_MAX - 1) / JDM_TACH_RPM_MAX);
}

/** 段角度几何:56 段扫 260°,起始 140°;返回第 i 段的起始角(度)。
 *  LVGL 弧角度为整数,四舍五入(而非截断)使节距 4/5° 交替、
 *  误差 ±0.5° 均匀分布,视觉等距;段末角恒 400°。 */
static inline int jdm_data_logic_tach_segment_start(int i)
{
    return 140 + (i * 260 + JDM_TACH_SEGMENTS / 2) / JDM_TACH_SEGMENTS;
}

/** 第 i 段起始位置对应的转速(段色带判定用)。 */
static inline int32_t jdm_data_logic_tach_seg_rpm(int i)
{
    return i * JDM_TACH_RPM_MAX / JDM_TACH_SEGMENTS;
}

/** 温度柱(60–120°C):返回柱内归一化高度 0..1000(‰),钳出界。 */
static inline int jdm_data_logic_thermal_permille(int32_t temp_c)
{
    if (temp_c <= 60) return 0;
    if (temp_c >= 120) return 1000;
    return (int)((temp_c - 60) * 1000 / 60);
}
