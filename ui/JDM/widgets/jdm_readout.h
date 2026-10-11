#pragma once
// ================================================================
//  jdm_readout.h — 读数卡(components.js readout(),四种尺寸)
//
//  label / value / unit 共用中轴 cx = x + width/2;y 是 label 顶边
//  (SVG dominant-baseline=hanging 语义)。value 用 Barlow(数字),
//  label/unit 用 Plex Mono。warning 态 value 变红,断连显示 "—"。
//  坐标为 720 母版值。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#include "core/jdm_data.h"
#include "core/jdm_tokens.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JDM_RO_PRIMARY = 0,   // w320  value180  hero
    JDM_RO_COMPACT,       // w230  value76
    JDM_RO_ECCENTRIC,     // w320  value144
    JDM_RO_ANALOG,        // w320  value96
} jdm_readout_size_t;

typedef struct {
    lv_obj_t *label;
    lv_obj_t *value;
    lv_obj_t *unit;
} jdm_readout_t;

/** 建读数卡。(mx,my) = label 顶边中点上方锚点(同原型 x,y 语义)。 */
void jdm_readout_create(lv_obj_t *parent, int32_t mx, int32_t my,
                        jdm_readout_size_t size, jdm_readout_t *out);

/** 设置 label/unit 文案(NULL = 不改动)。 */
void jdm_readout_set_label(jdm_readout_t *ro, const char *text);
void jdm_readout_set_unit(jdm_readout_t *ro, const char *text);

/** 设置值文本(内部脏检查,文本没变不重绘)。 */
void jdm_readout_set_value(jdm_readout_t *ro, const char *text);

/** 通道便捷入口:格式化 + 脏检查。invalid 显示 "--"。 */
void jdm_readout_set_channel(jdm_readout_t *ro, disp_item_t item, int32_t raw, bool valid);

/** 值着色:warning → 红;其余回 TEXT 角色(夜间自动换色)。 */
void jdm_readout_set_warning(jdm_readout_t *ro, bool warning);

#ifdef __cplusplus
}
#endif
