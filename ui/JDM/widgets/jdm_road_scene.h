#pragma once
// ================================================================
//  jdm_road_scene.h — 路景动效(speed 页;原型 roadScene())
//
//  每侧 5 个物体(树/建筑),相位随累计里程推进;透视投影
//  p²(远小近大)、两端淡出;停车/断连即停(续接累计距离)。
//  线稿近似:原型的填充形以轮廓线呈现(已知偏差)。
// ================================================================

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *jdm_road_scene_create(lv_obj_t *parent);

/** 推进并重绘。dt_ms 用于累计里程(mps×dt);moving=false 时只淡显。 */
void jdm_road_scene_update(lv_obj_t *scene, int32_t speed_kmh, bool moving, int32_t dt_ms);

#ifdef __cplusplus
}
#endif
