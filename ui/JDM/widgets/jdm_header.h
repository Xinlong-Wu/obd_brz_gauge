#pragma once
// ================================================================
//  jdm_header.h — 页头(components.js header())
//
//  标题 (center, 92) TITLE 字号 amber;副标题 (center, 135) SMALL muted。
//  坐标为 720 母版值,内部经 UIS() 折叠。
// ================================================================

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 在 parent 上建页头。sub 为空串/NULL 时只画标题。返回标题 label
 *  (页面想运行时改标题可直接用;不需要则忽略)。 */
lv_obj_t *jdm_header_create(lv_obj_t *parent, const char *title, const char *sub);

#ifdef __cplusplus
}
#endif
