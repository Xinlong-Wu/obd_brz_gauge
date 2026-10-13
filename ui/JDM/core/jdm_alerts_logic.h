#pragma once
// ================================================================
//  jdm_alerts_logic.h — 通道报警阈值纯逻辑(static inline,零 LVGL 依赖)
//
//  阈值取值:油温/水温/油压沿用固件状态字既有线(≥110°C、<1.0 bar
//  critical);转速/增压/电压/胎压/G 值对齐原型 scenarios.js 场景值
//  与表盘红区(增压红区起点 2.4 bar);文案与原型 alertReason 一致。
//  心率(HR 0x180D)与限速数据源未接入,对应判定暂缺。边界见
//  tests/test_jdm_alerts.c。
// ================================================================

#include <stdint.h>

#include "core/jdm_tokens.h"

// ---- 阈值(原始量纲:°C / 0.1bar / mV / 0.01g)----
// 转速两档(换挡提示/红区)来自车型配置 tach_cfg_t,由调用方以参数
// 传入 jdm_alerts_logic_rpm()(红区起点 = RPM critical,进红区即报)。
#define JDM_ALERTS_BAT_LOW     10800   // ≤ 电压过低(10.8 V)
#define JDM_ALERTS_BAT_HIGH    16400   // ≥ 电压过高(16.4 V)
#define JDM_ALERTS_TYRE_LOW    5       // ≤ 胎压过低 critical(0.5 bar)
#define JDM_ALERTS_TYRE_HIGH   34      // ≥ 胎压过高(3.4 bar)
#define JDM_ALERTS_G_LIMIT     100     // >  G 值提醒(1.0 g)

/* 编译期校验:配置对的大小关系把关——改宏时即时报错
 * (转速两档来自车型配置,关系由 vehicle_profile_get_tach_cfg() 钳制) */
_Static_assert(JDM_ALERTS_TYRE_LOW < JDM_ALERTS_TYRE_HIGH, "tyre low threshold must precede high");
_Static_assert(JDM_ALERTS_BAT_LOW < JDM_ALERTS_BAT_HIGH, "battery low threshold must precede high");

/** 判定转速通道;JDM_ALERT_NONE 时 reason 不写,否则置为静态文案。
 *  shift_rpm = 换挡提示(advisory,红区之前),limit_rpm = 红区起点
 *  (critical,= 车型配置 tach_cfg_t.red_rpm,进红区即报)。 */
static inline jdm_alert_level_t jdm_alerts_logic_rpm(int32_t rpm, int shift_rpm,
                                                     int limit_rpm, const char **reason)
{
    if (rpm >= limit_rpm) { *reason = "RPM LIMIT"; return JDM_ALERT_CRITICAL; }
    if (rpm >= shift_rpm) { *reason = "SHIFT"; return JDM_ALERT_ADVISORY; }
    return JDM_ALERT_NONE;
}

/** 油温过高:阈值 = 车型配置 oil_cfg_t.red_c(表盘红区起点)。 */
static inline jdm_alert_level_t jdm_alerts_logic_oil(int32_t raw, int red_c,
                                                     const char **reason)
{
    if (raw >= red_c) { *reason = "OIL TEMP HIGH"; return JDM_ALERT_WARNING; }
    return JDM_ALERT_NONE;
}

/** 油压过低:阈值 = 车型配置 oilp_cfg_t.low_x10(表盘量程下限之上);
 *  严格小于阈值才告警(= 阈值本身不触发,与既有状态字语义一致)。 */
static inline jdm_alert_level_t jdm_alerts_logic_oilp(int32_t raw, int low_x10,
                                                      const char **reason)
{
    if (raw < low_x10) { *reason = "OIL PRESS LOW"; return JDM_ALERT_CRITICAL; }
    return JDM_ALERT_NONE;
}

/** 增压过高:阈值 = 车型配置 boost_cfg_t.red_x10(表盘红区起点)。 */
static inline jdm_alert_level_t jdm_alerts_logic_boost(int32_t raw, int red_x10,
                                                       const char **reason)
{
    if (raw >= red_x10) { *reason = "BOOST HIGH"; return JDM_ALERT_WARNING; }
    return JDM_ALERT_NONE;
}

/** 水温过高:阈值 = 车型配置 clt_cfg_t.red_c(表盘红区起点)。 */
static inline jdm_alert_level_t jdm_alerts_logic_clt(int32_t raw, int red_c,
                                                     const char **reason)
{
    if (raw >= red_c) { *reason = "COOLANT HIGH"; return JDM_ALERT_WARNING; }
    return JDM_ALERT_NONE;
}

static inline jdm_alert_level_t jdm_alerts_logic_bat(int32_t raw_mv, const char **reason)
{
    if (raw_mv <= JDM_ALERTS_BAT_LOW) { *reason = "VOLTAGE LOW"; return JDM_ALERT_WARNING; }
    if (raw_mv >= JDM_ALERTS_BAT_HIGH) { *reason = "VOLTAGE HIGH"; return JDM_ALERT_WARNING; }
    return JDM_ALERT_NONE;
}

/** 单轮胎压(轮位前缀由页面拼装)。 */
static inline jdm_alert_level_t jdm_alerts_logic_tyre(int32_t raw)
{
    if (raw <= JDM_ALERTS_TYRE_LOW) return JDM_ALERT_CRITICAL;
    if (raw >= JDM_ALERTS_TYRE_HIGH) return JDM_ALERT_WARNING;
    return JDM_ALERT_NONE;
}

/** G 值提醒:横向或纵向任一越限(原型 limits.gforce,advisory)。 */
static inline jdm_alert_level_t jdm_alerts_logic_gforce(int32_t lat, int32_t lon,
                                                        const char **reason)
{
    int32_t a = lat < 0 ? -lat : lat;
    int32_t b = lon < 0 ? -lon : lon;
    if (a > JDM_ALERTS_G_LIMIT || b > JDM_ALERTS_G_LIMIT) {
        *reason = "G LIMIT";
        return JDM_ALERT_ADVISORY;
    }
    return JDM_ALERT_NONE;
}

/** 按严重度降序插入(critical 优先,同级保持调用先后),返回新条数。
 *  满员或 NONE 时原样返回 n。 */
static inline int jdm_alerts_logic_push(jdm_alert_item_t *arr, int n, int cap,
                                        const char *reason, jdm_alert_level_t lv)
{
    if (lv == JDM_ALERT_NONE || n >= cap) return n;
    int j = n;
    while (j > 0 && arr[j - 1].level < lv) j--;
    for (int k = n; k > j; k--) arr[k] = arr[k - 1];
    arr[j].reason = reason;
    arr[j].level = lv;
    return n + 1;
}
