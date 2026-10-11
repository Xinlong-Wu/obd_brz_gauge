#pragma once
// ================================================================
//  jdm_trend.h — 折线 sparkline(components.js trend(),heart 页等)
//
//  样本归一化到给定矩形(值域 = 样本 min..max,全等值时画顶线),
//  线宽 3 圆帽。点阵缓冲在 create 时按 max_n 分配、line 删除时释放。
// ================================================================

#include <stdint.h>
#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *line;     // sparkline 对象(lv_line)
    uint16_t max_n;     // 点阵容量
    int32_t mx, my, mw, mh;   // 母版几何
} jdm_trend_t;

/** 建 sparkline(空数据 = 不画)。 */
void jdm_trend_create(lv_obj_t *parent, int32_t mx, int32_t my,
                      int32_t mw, int32_t mh, uint16_t max_n, jdm_trend_t *out);

/** 整组替换样本并重算归一化(脏检查:样本全等则不重画)。n≤max_n。 */
void jdm_trend_set(jdm_trend_t *t, const int32_t *vals, uint16_t n);

void jdm_trend_set_color(jdm_trend_t *t, jdm_color_role_t role);

#ifdef __cplusplus
}
#endif
