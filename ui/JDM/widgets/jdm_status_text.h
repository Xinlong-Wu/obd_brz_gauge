#pragma once
// ================================================================
//  jdm_status_text.h — 状态字(components.js statusText())
//  normal→绿 / warning→红 / disconnected→muted。坐标 720 母版。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JDM_STATUS_NORMAL = 0,
    JDM_STATUS_WARNING,
    JDM_STATUS_DISCONNECTED,
} jdm_status_state_t;

lv_obj_t *jdm_status_text_create(lv_obj_t *parent, int32_t mx, int32_t my, const char *text);

/** 更新文本与颜色(NULL 文本 = 只改状态色)。 */
void jdm_status_text_set(lv_obj_t *st, const char *text, jdm_status_state_t state);

#ifdef __cplusplus
}
#endif
