// ================================================================
//  page_oilpress.c — 发动机油压(原型 05 页,偏心机械表)
//
//  左侧偏心表(圆心 220,355,r160 轨道 w20,130° 起扫 260°,0..6 bar)
//  + 右侧 eccentric 读数区(cx520)。
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
#include "widgets/jdm_header.h"
#include "widgets/jdm_needle.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"
#include "widgets/jdm_status_text.h"

#include <stdio.h>

static lv_obj_t *s_arc;
static lv_obj_t *s_needle;
static jdm_readout_t s_ro;
static lv_obj_t *s_status;

#define PCX 220   // 表盘圆心(母版)
#define PCY 355

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "OIL PRESSURE", "LUBRICATION");

    // 轨道 r160 w20(130°..390°)
    lv_obj_t *track = lv_arc_create(page);
    int32_t size = UIS(2 * 160 + 20);
    lv_obj_set_size(track, size, size);
    lv_obj_set_pos(track, UIS(PCX) - size / 2, UIS(PCY) - size / 2);
    lv_obj_remove_style(track, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(track, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(track, jdm_color(JDM_ROLE_PANEL), LV_PART_MAIN);
    lv_obj_set_style_arc_width(track, UIS(20), LV_PART_MAIN);
    lv_arc_set_rotation(track, 0);
    lv_arc_set_bg_angles(track, 130, 390);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    // 刻度:130° 起每 13° 共 21 根,w3,轨道外缘 r157..180
    for (int i = 0; i <= 20; i++) {
        jdm_draw_tick(page, PCX, PCY, 157, 180, 130 + i * 13, 3, JDM_ROLE_LINE);
    }
    // 标签 0..6(内圈 r132)
    for (int v = 0; v <= 6; v++) {
        jdm_pt_t p = jdm_draw_polar(PCX, PCY, 132, 130 + v * 260 / 6);
        char txt[4];
        snprintf(txt, sizeof(txt), "%d", v);
        jdm_text(page, p.x, p.y - 12, txt, JDM_FONT_LABEL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    s_arc = jdm_arc_create(page, PCX, PCY, 160, 8);
    jdm_arc_configure(s_arc, 130, 260, 60);   // 0..6 bar → 0..60(0.1bar)
    s_needle = jdm_needle_create_group(page, PCX, PCY, 125, JDM_ROLE_TEXT);

    // 右侧读数区(cx = 360+160 = 520)
    jdm_readout_create(page, 520, 240, JDM_RO_ECCENTRIC, &s_ro);
    jdm_readout_set_label(&s_ro, "PRESSURE");
    jdm_readout_set_unit(&s_ro, "bar");

    s_status = jdm_status_text_create(page, 520, 540, "WAITING");
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_OILP, &raw) && jdm_conn_active();

    if (valid) {
        jdm_arc_set(s_arc, raw);   // 0..60(0.1bar),量程内自动钳
        int deg = 130 + raw * 260 / 60;
        jdm_needle_set_angle(s_needle, deg);
    } else {
        lv_obj_set_hidden(s_arc, true);
        lv_obj_set_hidden(s_needle, true);
    }
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OILP, raw, valid);
    jdm_readout_set_value(&s_ro, buf);

    if (!valid) {
        jdm_status_text_set(s_status, "WAITING", JDM_STATUS_DISCONNECTED);
    } else if (raw < 10) {   // <1.0 bar 怠速低压告警
        jdm_status_text_set(s_status, "LOW PRESSURE", JDM_STATUS_WARNING);
    } else {
        jdm_status_text_set(s_status, "STABLE", JDM_STATUS_NORMAL);
    }
}

const jdm_page_t page_oilpress = {"oilpress", create, update, 33};
