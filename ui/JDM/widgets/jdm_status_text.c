// ================================================================
//  jdm_status_text.c — 状态字实现
// ================================================================

#include "jdm_status_text.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"

static const jdm_color_role_t s_state_ink[JDM_STATUS_DISCONNECTED + 1] = {
    [JDM_STATUS_NORMAL]       = JDM_ROLE_GREEN,
    [JDM_STATUS_WARNING]      = JDM_ROLE_RED,
    [JDM_STATUS_DISCONNECTED] = JDM_ROLE_MUTED,
};

lv_obj_t *jdm_status_text_create(lv_obj_t *parent, int32_t mx, int32_t my, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, jdm_font(JDM_FONT_SUPPORT), 0);
    lv_obj_set_style_text_color(l, jdm_color(JDM_ROLE_GREEN), 0);
    lv_label_set_text(l, text ? text : "");
    // parent = 全屏页容器:母版 x 相对中心 (360) 折算
    lv_obj_align(l, LV_ALIGN_TOP_MID, UIS(mx - 360), UIS(my));
    return l;
}

void jdm_status_text_set(lv_obj_t *st, const char *text, jdm_status_state_t state)
{
    if (text) {
        lv_label_set_text(st, text);
    }
    lv_obj_set_style_text_color(st, jdm_color(s_state_ink[state]), 0);
}
