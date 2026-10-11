#pragma once
// ================================================================
//  jdm_tach.h — 56 段转速弧(components.js tachometer())
//
//  56 段短弧,起始 140° 扫 260°,半径 282;段宽 major(i%6==0)=18、
//  其余 10。点亮规则:ceil(rpm/8000×56),i≥45 红、i≥40 琥珀、
//  其余暖白;未亮 = line 色。断连全灭。
// ================================================================

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建 56 段(挂在 parent,圆心即表盘中心)。返回容器。 */
lv_obj_t *jdm_tach_create(lv_obj_t *parent);

/** 更新点亮数与配色(内部按段脏检查)。 */
void jdm_tach_update(lv_obj_t *tach, int32_t rpm, bool connected);

#ifdef __cplusplus
}
#endif
