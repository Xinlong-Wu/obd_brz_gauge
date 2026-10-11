#pragma once
// ================================================================
//  jdm_thermal.h — 纵向温度柱(coolant 页,art('thermal')+动态柱)
//
//  轨道 rect(142,200,44×314) rx22 panel;动态柱 x154 w20 rx10 amber
//  (报警变红),柱顶横帽 x137 宽 49 厚 3;右侧刻度线 13 根(120/115/110
//  红色,每 15°C 一根长刻度);"120"/"60" 标签属页面(数值字号不同),
//  不在本组件。坐标 720 母版。
// ================================================================

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建温度柱集群(轨道 + 刻度)。返回容器(建动态柱用句柄)。 */
lv_obj_t *jdm_thermal_create(lv_obj_t *parent);

/** 更新动态柱与顶帽。temp 无效时隐藏柱。报警时柱/帽变红。 */
void jdm_thermal_update(lv_obj_t *thermal, int32_t temp_c, bool valid, bool warning);

#ifdef __cplusplus
}
#endif
