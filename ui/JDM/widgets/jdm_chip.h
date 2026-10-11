#pragma once
// ================================================================
//  jdm_chip.h — 状态小条(components.js chip())
//  高 44 rx8 panel 底 + 居中 small 文字;amber/red 底时文字用 face 色。
// ================================================================

#include "lvgl.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *jdm_chip_create(lv_obj_t *parent, int32_t mx, int32_t my,
                          int32_t mw, const char *text, jdm_color_role_t bg);

void jdm_chip_set(lv_obj_t *chip, const char *text, jdm_color_role_t bg);

#ifdef __cplusplus
}
#endif
