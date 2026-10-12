#pragma once
// ================================================================
//  jdm_thermal.h — 纵向温度柱(coolant 页,art('thermal')+动态柱)
//
//  轨道 rect(142,200,44×314) rx22 panel;动态柱 x154 w20 rx10 暖白
//  (报警变红,车主约定;原型为琥珀),柱顶横帽 x137 宽 49 厚 3;右侧
//  刻度线 13 根(≥红区阈值的红色);"120"/"60" 标签属页面(数值字号
//  不同),不在本组件。量程/红区阈值来自车型配置(clt_cfg_t)。
//  坐标 720 母版。
// ================================================================

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 建温度柱集群(轨道 + 刻度)。量程与红区阈值来自车型配置
 *  (vehicle_profile_get_clt_cfg()):刻度按起始温度 ≥ red_c 标红。
 *  返回容器(建动态柱用句柄)。 */
lv_obj_t *jdm_thermal_create(lv_obj_t *parent);

/** 更新动态柱与顶帽。temp 无效时隐藏柱。报警时柱/帽变红。 */
void jdm_thermal_update(lv_obj_t *thermal, int32_t temp_c, bool valid, bool warning);

#ifdef __cplusplus
}
#endif
