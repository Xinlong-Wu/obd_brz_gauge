#pragma once
// ================================================================
//  jdm_toggle.h — 开关(components.js toggleControl(),w 通常 104)
//  轨道 (x,y+10,104×38) rx4 panel 底 line 描边 + 中线竖线 + 拨片
//  32×28(on 靠右 amber / off 靠左 muted)+ 下方 ON/OFF small 字。
//  点击区 = (x,y,w,88)。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *jdm_toggle_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                            bool on);

/** 设状态(拨片位置/颜色/文字)。 */
void jdm_toggle_set(lv_obj_t *toggle, bool on);

#ifdef __cplusplus
}
#endif
