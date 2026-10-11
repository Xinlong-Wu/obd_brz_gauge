// ================================================================
//  jdm_alert.c — 报警呈现实现
// ================================================================

#include "jdm_alert.h"

#include <stdio.h>
#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"

#define RIM_RADIUS  330
#define RIM_WIDTH   JDM_ALERT_RIM_WIDTH

static lv_obj_t *rim_segment_create(lv_obj_t *parent, int a_start, int a_end)
{
    lv_obj_t *arc = lv_arc_create(parent);
    int32_t size = UIS(2 * RIM_RADIUS + RIM_WIDTH);
    lv_obj_set_size(arc, size, size);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, UIS(RIM_WIDTH), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, jdm_color(JDM_ROLE_RED), LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_bg_angles(arc, a_start, a_end);
    lv_arc_set_range(arc, 0, 1);
    lv_arc_set_value(arc, 1);
    return arc;
}

void jdm_alert_create(lv_obj_t *parent, int32_t footer_my, jdm_alert_widgets_t *out)
{
    out->msg = lv_label_create(parent);
    lv_obj_set_style_text_font(out->msg, jdm_font(JDM_FONT_SMALL), 0);
    lv_obj_set_style_text_color(out->msg, jdm_color(JDM_ROLE_RED), 0);
    lv_label_set_text(out->msg, "");
    lv_obj_align(out->msg, LV_ALIGN_TOP_MID, 0, UIS(footer_my ? footer_my : JDM_ALERT_FOOTER_Y));

    out->rim = lv_obj_create(parent);
    lv_obj_remove_style_all(out->rim);
    lv_obj_set_size(out->rim, UIS(720), UIS(720));
    lv_obj_set_pos(out->rim, 0, 0);
    lv_obj_clear_flag(out->rim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_hidden(out->rim, true);
    // 两段外缘弧:左下 145..215,右下 −35..35(SVG 负角 = 360+角)
    rim_segment_create(out->rim, 145, 215);
    rim_segment_create(out->rim, 325, 395);
}

void jdm_alert_set(const jdm_alert_widgets_t *w, const jdm_alert_item_t *alerts, uint8_t n)
{
    // ---- 页脚摘要 ----
    if (alerts && n > 0) {
        char buf[64];
        if (n > 1) {
            snprintf(buf, sizeof(buf), "%s +%d", alerts[0].reason, n - 1);
        } else {
            snprintf(buf, sizeof(buf), "%s", alerts[0].reason);
        }
        if (strcmp(lv_label_get_text(w->msg), buf) != 0) {
            lv_label_set_text(w->msg, buf);
        }
        jdm_color_role_t ink = (alerts[0].level == JDM_ALERT_ADVISORY)
                                   ? JDM_ROLE_AMBER : JDM_ROLE_RED;
        lv_obj_set_style_text_color(w->msg, jdm_color(ink), 0);
    } else if (lv_label_get_text(w->msg)[0] != '\0') {
        lv_label_set_text(w->msg, "");
    }

    // ---- critical 外缘弧 ----
    bool critical = false;
    for (uint8_t i = 0; alerts && i < n; i++) {
        if (alerts[i].level == JDM_ALERT_CRITICAL) {
            critical = true;
            break;
        }
    }
    if (critical) {
        lv_obj_set_hidden(w->rim, false);
    } else {
        lv_obj_set_hidden(w->rim, true);
    }
}
