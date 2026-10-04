#pragma once
// ================================================================
//  ui_disp_item_logic.h — 数据项系统纯逻辑(static inline,零依赖)
//
//  从 ui_disp_item.c 抽出的可测逻辑:自适应平滑步进、报警阈值判定。
//  只依赖 stdint/stdbool,tests/ 直接编译断言(见 test_ui_disp_item.c);
//  UI/IO 相关的部分(缓存读取、label 更新)留在 .c 里。
// ================================================================

#include <stdint.h>
#include <stdbool.h>

/** 报警"关闭"哨兵:阈值等于它表示该项不启用报警(nvs_chart_alarm 约定)。 */
#define UI_DISP_ITEM_ALARM_OFF 32767

/**
 * 自适应步进:displayed 向 target 逼近一步。
 * |diff| ≤ threshold 时每次走 ±1(慢速跟随);|diff| > threshold 时按
 * ~1/3 比例逼近(最少 2 步),避免大跳变时数值"卡住"。
 */
static inline int32_t ui_disp_item_anim_step_i32(int32_t displayed,
                                                 int32_t target,
                                                 int32_t threshold)
{
    int32_t diff = target - displayed;
    if (diff == 0) return displayed;
    int32_t abs_diff = (diff > 0) ? diff : -diff;
    int32_t step = (diff > 0) ? 1 : -1;

    if (abs_diff > threshold) {
        int32_t rapid = abs_diff / 3;     /* eat ~33% of the gap per tick */
        if (rapid < 2) rapid = 2;          /* minimum 2 steps */
        if (rapid > abs_diff) rapid = abs_diff;
        step = (diff > 0) ? rapid : -rapid;
    }
    return displayed + step;
}

/**
 * 报警判定:值有效、阈值已启用(≠哨兵)且越过阈值时为真。
 * 调用方负责 30s 冷却(OILP/BKT)与着色。
 */
static inline bool ui_disp_item_alarm_over_threshold(int16_t threshold,
                                                     int32_t value,
                                                     bool valid)
{
    return valid && threshold < UI_DISP_ITEM_ALARM_OFF &&
           value >= (int32_t)threshold;
}
