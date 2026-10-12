#pragma once
// ================================================================
//  jdm_tach.h — 56 段转速弧(components.js tachometer())
//
//  56 段短弧,起始 140° 扫 260°,半径 282;段宽 major(i%6==0)=18、
//  其余 10。点亮规则:ceil(rpm/量程×56),段起始转速落在红区→红、
//  琥珀区→琥珀、其余暖白;未亮 = line 色。断连全灭。
//  量程/区间来自车型配置(jdm_tach_apply_zones,见 tach_cfg_t)。
// ================================================================

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建 56 段(挂在 parent,圆心即表盘中心)。返回容器。 */
lv_obj_t *jdm_tach_create(lv_obj_t *parent);

/** 应用车型配置:量程 / 琥珀区起点 / 红区起点(母版 rpm)。
 *  在 create 之后、首次 update 之前调用。 */
void jdm_tach_apply_zones(int32_t rpm_max, int32_t amber_rpm, int32_t red_rpm);

/** 更新点亮数与配色(内部按段脏检查)。 */
void jdm_tach_update(lv_obj_t *tach, int32_t rpm, bool connected);

#ifdef __cplusplus
}
#endif
