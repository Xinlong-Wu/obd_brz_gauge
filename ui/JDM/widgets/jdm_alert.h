#pragma once
// ================================================================
//  jdm_alert.h — 报警呈现(alertMessage + alertRim)
//
//  页脚摘要 (center, 638) SMALL:第一条原因(多项加 " +N"),色取
//  第一条级别(advisory→amber,warning/critical→red)。
//  critical 时另有外缘双红弧(r330 宽 4 圆帽,145°..215° 与 −35°..35°)。
// ================================================================

#include <stdint.h>
#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *reason;        // 短文案(如 "BOOST HIGH")
    jdm_alert_level_t level;   // tokens.h 三级
} jdm_alert_item_t;

typedef struct {
    lv_obj_t *msg;    // 页脚摘要 label
    lv_obj_t *rim;    // critical 外缘双弧容器
} jdm_alert_widgets_t;

/** 建页脚摘要(页脚 y 为母版值,默认 638)+ 外缘弧(初始隐藏)。 */
void jdm_alert_create(lv_obj_t *parent, int32_t footer_my, jdm_alert_widgets_t *out);

/** 更新(内部脏检查)。alerts=NULL/0 = 清除。 */
void jdm_alert_set(const jdm_alert_widgets_t *w, const jdm_alert_item_t *alerts, uint8_t n);

#ifdef __cplusplus
}
#endif
