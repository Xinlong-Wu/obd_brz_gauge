// ================================================================
//  page_boost.c — 增压 / 真空(原型 02 页 MANIFOLD)
//
//  270° 压力弧(r274 轨道 w30)+ 放射刻度 + 细指针(r205),
//  量程 −1..3 bar;读数偏下居中。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_fonts.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_arc.h"
#include "widgets/jdm_needle.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"

#include <stdio.h>

static lv_obj_t *s_arc;
static lv_obj_t *s_needle;
static jdm_readout_t s_ro;
static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *page)
{
    // 文字与读数垫底(用户约定:指针遮盖文字,与原型层叠相反);
    // 弧与指针最后建,压在最上层
    jdm_text(page, 360, 178, "MANIFOLD", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    jdm_text(page, 360, 325, "VACUUM / BOOST", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_readout_create(page, 360, 375, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "BOOST");
    jdm_readout_set_unit(&s_ro, "bar");

    // 轨道 r274 w30(135°..405°)
    lv_obj_t *track = lv_arc_create(page);
    int32_t size = UIS(2 * 274 + 30);
    lv_obj_set_size(track, size, size);
    lv_obj_set_pos(track, UIS(360) - size / 2, UIS(365) - size / 2);
    lv_obj_remove_style(track, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(track, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(track, jdm_color(JDM_ROLE_PANEL), LV_PART_MAIN);
    lv_obj_set_style_arc_width(track, UIS(30), LV_PART_MAIN);
    lv_arc_set_rotation(track, 0);
    lv_arc_set_bg_angles(track, 135, 405);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    // 41 根放射刻度(135° 起每 6.75°,外端固定 r290):major 每 5 根向内
    // 伸至 r263,minor r280..290;三段配色:≤+1.2bar text、+1.3..2.3 line、
    // ≥2.4 红(与原型 art('boost') 一致)
    for (int i = 0; i <= 40; i++) {
        float deg = 135.0f + i * 27.0f / 4.0f;   // 6.75°
        bool major = (i % 5 == 0);
        int32_t r1 = major ? 263 : 280;
        jdm_color_role_t c = (i >= 34) ? JDM_ROLE_RED
                           : (i >= 23) ? JDM_ROLE_LINE : JDM_ROLE_TEXT;
        jdm_draw_tick(page, 360, 365, r1, 290, deg, major ? 4 : 2, c);
    }
    // 刻度数字 −1..3(support 号,muted):中心按原型落在 r242 同心环
    // (art.js 坐标为 SVG 基线语义,需按数字中心极坐标放置,垂直用
    // 字体 line_height 居中)
    const lv_font_t *num_font = jdm_font(JDM_FONT_SUPPORT);
    int32_t half_lh = num_font->line_height / 2;
    for (int v = -1; v <= 3; v++) {
        jdm_pt_t p = jdm_draw_polar(360, 365, 242, 135 + (v + 1) * 270 / 4);
        char txt[8];   // "−1" = 3 字节 UTF-8 负号 + 数字 + NUL
        if (v == -1) {
            snprintf(txt, sizeof(txt), "\xE2\x88\x92" "1");
        } else {
            snprintf(txt, sizeof(txt), "%d", v);
        }
        jdm_text(page, p.x, p.y - half_lh, txt,
                 JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    s_arc = jdm_arc_create(page, 360, 365, 274, 12);
    jdm_arc_configure(s_arc, 135, 270, 40);   // bar×10,−1..3
    s_needle = jdm_needle_create_group(page, 360, 365, 205, JDM_ROLE_AMBER);

    jdm_alert_create(page, 0, &s_alert);
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_BOOST, &raw) && jdm_conn_active();

    if (valid) {
        // 量程 −1..3 bar:deg = 135+(raw+10)*6.75(浮点,0.1bar 步进精确)
        float deg = 135.0f + (raw + 10) * 6.75f;
        jdm_arc_set(s_arc, raw + 10);
        jdm_needle_set_angle(s_needle, deg);
    } else {
        lv_obj_set_hidden(s_arc, true);
        lv_obj_set_hidden(s_needle, true);
    }
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, raw, valid);
    jdm_readout_set_value(&s_ro, buf);

    jdm_alert_item_t item;
    int n = 0;
    const char *reason = NULL;
    jdm_alert_level_t lv = JDM_ALERT_NONE;
    if (valid) {
        lv = jdm_alerts_logic_boost(raw, &reason);
        n = jdm_alerts_logic_push(&item, n, 1, reason, lv);
    }
    bool alarm = lv != JDM_ALERT_NONE;
    // 弧与指针(原型 ink('boost'))超限同变警告色
    jdm_arc_set_color(s_arc, alarm ? JDM_ROLE_RED : JDM_ROLE_AMBER);
    jdm_needle_set_color(s_needle, alarm ? JDM_ROLE_RED : JDM_ROLE_AMBER);
    jdm_readout_set_warning(&s_ro, alarm);
    jdm_alert_set(&s_alert, &item, n);
}

const jdm_page_t page_boost = {"boost", create, update, 33};
