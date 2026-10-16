// ================================================================
//  page_speed.c — 车速 / 巡航(原型 09 页)
//
//  路景透视动效(车速联动)+ 透视线 + 主读数 + 巡航状态 chip。
// ================================================================

#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_chip.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"
#include "widgets/jdm_road_scene.h"

#include "app_obd_dsp/vehicle_profiles.h"

#include <stdio.h>

static lv_obj_t *s_scene;
static jdm_readout_t s_ro;
static lv_obj_t *s_chip;
static lv_obj_t *s_set_text;
static speed_cfg_t s_speed_cfg;   // 当前车型的巡航设定/限速(NVS 车型选择)

static void create(lv_obj_t *page)
{
    s_speed_cfg = *vehicle_profile_get_speed_cfg();   // 车型的巡航设定/限速
    jdm_header_create(page, "SPEED", "ROADSTER / TOURING");
    s_scene = jdm_road_scene_create(page);

    // 透视路缘 + 中线刻度
    static const jdm_pt_t edges[4] = {{158, 575}, {292, 380}, {562, 575}, {428, 380}};
    jdm_pt_t l1[2] = {{edges[0].x, edges[0].y}, {edges[1].x, edges[1].y}};
    jdm_draw_poly(page, l1, 2, 3, JDM_ROLE_LINE);
    jdm_pt_t l2[2] = {{edges[2].x, edges[2].y}, {edges[3].x, edges[3].y}};
    jdm_draw_poly(page, l2, 2, 3, JDM_ROLE_LINE);
    static const jdm_pt_t dash1[] = {{360, 550}, {360, 524}};
    jdm_draw_poly(page, dash1, 2, 5, JDM_ROLE_AMBER);
    static const jdm_pt_t dash2[] = {{360, 490}, {360, 474}};
    jdm_draw_poly(page, dash2, 2, 5, JDM_ROLE_AMBER);

    jdm_readout_create(page, 360, 204, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "VELOCITY");
    jdm_readout_set_unit(&s_ro, "km/h");

    s_chip = jdm_chip_create(page, 224, 490, 272, "CRUISE STANDBY", JDM_ROLE_PANEL);
    s_set_text = jdm_text(page, 360, 551, "SET -- km/h", JDM_FONT_TITLE, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
}

static void update(void)
{
    static int32_t last_ms = 0;
    int32_t now = lv_tick_get();
    int32_t dt = last_ms ? now - last_ms : 33;
    last_ms = now;

    int32_t speed = 0;
    bool speed_ok = jdm_data_read_cache(DISP_ITEM_SPEED, &speed);
    bool conn = jdm_conn_active();
    bool moving = conn && speed_ok && speed > 0;

    jdm_road_scene_update(s_scene, speed, moving, dt);

    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_SPEED, speed, conn && speed_ok);
    jdm_readout_set_value(&s_ro, buf);

    bool overspeed = conn && speed_ok && speed > s_speed_cfg.speed_limit;

    if (!conn) {
        jdm_chip_set(s_chip, "CRUISE UNAVAILABLE", JDM_ROLE_PANEL);   // 原型断连带为 panel
        lv_label_set_text(s_set_text, "SET \xE2\x80\x94 km/h");
        lv_obj_set_style_text_color(s_set_text, jdm_color(JDM_ROLE_MUTED), 0);
    } else if (overspeed) {
        // 超速:限速提示(原型 speed-limit 场景:琥珀 chip + LIMIT 文字)
        jdm_chip_set(s_chip, "SPEED LIMIT", JDM_ROLE_AMBER);
        snprintf(buf, sizeof(buf), "LIMIT %d km/h", (int)s_speed_cfg.speed_limit);
        lv_label_set_text(s_set_text, buf);
        lv_obj_set_style_text_color(s_set_text, jdm_color(JDM_ROLE_AMBER), 0);
    } else if (speed == 0) {
        jdm_chip_set(s_chip, "CRUISE STANDBY", JDM_ROLE_PANEL);
        snprintf(buf, sizeof(buf), "SET %d km/h", (int)s_speed_cfg.cruise_set);
        lv_label_set_text(s_set_text, buf);
        lv_obj_set_style_text_color(s_set_text, jdm_color(JDM_ROLE_MUTED), 0);
    } else {
        jdm_chip_set(s_chip, "CRUISE ACTIVE", JDM_ROLE_PANEL);
        snprintf(buf, sizeof(buf), "SET %d km/h", (int)s_speed_cfg.cruise_set);
        lv_label_set_text(s_set_text, buf);
        lv_obj_set_style_text_color(s_set_text, jdm_color(JDM_ROLE_GREEN), 0);
    }
}

const jdm_page_t page_speed = {"speed", create, update, 33};
