// ================================================================
//  page_boost.c — 增压 / 真空(原型 02 页 MANIFOLD)
//
//  270° 压力弧(r274 轨道 w30)+ 放射刻度 + 细指针(r205),
//  量程 −1..3 bar;读数偏下居中。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
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

    // 21 根放射刻度(135° 起每 13.5°;major 每 5 根加长;text 色,高压区红)
    for (int i = 0; i <= 20; i++) {
        int deg = 135 + i * 27 / 2;   // 13.5°
        int32_t r2 = (i % 5 == 0) ? 294 : 278;
        jdm_color_role_t c = (i >= 17) ? JDM_ROLE_RED : JDM_ROLE_TEXT;
        jdm_draw_tick(page, 360, 365, 267, r2, deg, (i % 5 == 0) ? 4 : 2, c);
    }

    s_arc = jdm_arc_create(page, 360, 365, 274, 12);
    jdm_arc_configure(s_arc, 135, 270, 40);   // bar×10,−1..3
    s_needle = jdm_needle_create_group(page, 360, 365, 205, JDM_ROLE_AMBER);

    jdm_text(page, 360, 178, "MANIFOLD", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    jdm_text(page, 360, 325, "VACUUM / BOOST", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    jdm_readout_create(page, 360, 375, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "BOOST");
    jdm_readout_set_unit(&s_ro, "bar");

    jdm_alert_create(page, 0, &s_alert);
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_BOOST, &raw) && jdm_conn_active();

    if (valid) {
        // 量程 −1..3 bar:deg = 135+(raw+10)*270/40(raw 0.1bar)
        int deg = 135 + (raw + 10) * 270 / 40;
        jdm_arc_set(s_arc, raw + 10);
        jdm_needle_set_angle(s_needle, deg);
    } else {
        lv_obj_set_hidden(s_arc, true);
        lv_obj_set_hidden(s_needle, true);
    }
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, raw, valid);
    jdm_readout_set_value(&s_ro, buf);
}

const jdm_page_t page_boost = {"boost", create, update, 33};
