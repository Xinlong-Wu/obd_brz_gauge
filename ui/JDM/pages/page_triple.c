// ================================================================
//  page_triple.c — 三项综合监测(原型 06 页 POWERTRAIN)
//
//  顶部增压横条 + 主读数;分隔线下油温/油压双 compact 读数。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"

#include <stdio.h>

static lv_obj_t *s_boost_bar;
static jdm_readout_t s_boost_ro;
static jdm_readout_t s_oil_ro;
static jdm_readout_t s_press_ro;
static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "POWERTRAIN", "THREE CHANNEL VIEW");

    // art('triple'):分隔线 + 增压轨道槽
    jdm_pt_t h1[2] = {{100, 478}, {620, 478}};
    jdm_draw_poly(page, h1, 2, 2, JDM_ROLE_LINE);
    jdm_pt_t v1[2] = {{360, 482}, {360, 620}};
    jdm_draw_poly(page, v1, 2, 2, JDM_ROLE_LINE);
    lv_obj_t *slot = lv_obj_create(page);
    lv_obj_remove_style_all(slot);
    lv_obj_set_pos(slot, UIS(112), UIS(196));
    lv_obj_set_size(slot, UIS(496), UIS(10));
    lv_obj_set_style_radius(slot, UIS(5), 0);
    lv_obj_set_style_bg_color(slot, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(slot, LV_OPA_COVER, 0);

    // 动态增压条(同槽位,amber,宽度 = (boost+1)/4 × 496)
    s_boost_bar = lv_obj_create(page);
    lv_obj_remove_style_all(s_boost_bar);
    lv_obj_set_pos(s_boost_bar, UIS(112), UIS(196));
    lv_obj_set_size(s_boost_bar, 1, UIS(10));
    lv_obj_set_style_radius(s_boost_bar, UIS(5), 0);
    lv_obj_set_style_bg_color(s_boost_bar, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(s_boost_bar, LV_OPA_COVER, 0);

    // 主读数(cx360)
    jdm_readout_create(page, 360, 215, JDM_RO_PRIMARY, &s_boost_ro);
    jdm_readout_set_label(&s_boost_ro, "BOOST");
    jdm_readout_set_unit(&s_boost_ro, "bar");

    // 下方双 compact(cx = x+115)
    jdm_readout_create(page, 245, 492, JDM_RO_COMPACT, &s_oil_ro);
    jdm_readout_set_label(&s_oil_ro, "OIL TEMP");
    jdm_readout_set_unit(&s_oil_ro, "\xC2\xB0" "C");
    jdm_readout_create(page, 485, 492, JDM_RO_COMPACT, &s_press_ro);
    jdm_readout_set_label(&s_press_ro, "OIL PRESS");
    jdm_readout_set_unit(&s_press_ro, "bar");

    jdm_alert_create(page, 650, &s_alert);
}

static void update(void)
{
    int32_t boost = 0, oil = 0, press = 0;
    bool conn = jdm_conn_active();
    bool boost_ok = jdm_data_read_cache(DISP_ITEM_BOOST, &boost) && conn;
    bool oil_ok = jdm_data_read_cache(DISP_ITEM_OIL, &oil) && conn;
    bool press_ok = jdm_data_read_cache(DISP_ITEM_OILP, &press) && conn;

    if (boost_ok) {
        int32_t frac = boost + 10;          // −1..3 bar → 0..40
        if (frac < 0) frac = 0;
        if (frac > 40) frac = 40;
        lv_obj_set_width(s_boost_bar, UIS(496) * frac / 40);
    } else {
        lv_obj_set_width(s_boost_bar, 0);
    }

    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, boost, boost_ok);
    jdm_readout_set_value(&s_boost_ro, buf);
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OIL, oil, oil_ok);
    jdm_readout_set_value(&s_oil_ro, buf);
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OILP, press, press_ok);
    jdm_readout_set_value(&s_press_ro, buf);

    // 报警:增压 / 油温 / 油压(critical 优先;断连抑制)
    jdm_alert_item_t items[3];
    int n = 0;
    const char *reason = NULL;
    jdm_alert_level_t lv;
    lv = boost_ok ? jdm_alerts_logic_boost(boost, vehicle_profile_get_boost_cfg()->red_x10, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 3, reason, lv);
    jdm_readout_set_warning(&s_boost_ro, lv != JDM_ALERT_NONE);
    lv = oil_ok ? jdm_alerts_logic_oil(oil, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 3, reason, lv);
    jdm_readout_set_warning(&s_oil_ro, lv != JDM_ALERT_NONE);
    lv = press_ok ? jdm_alerts_logic_oilp(press, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 3, reason, lv);
    jdm_readout_set_warning(&s_press_ro, lv != JDM_ALERT_NONE);
    jdm_alert_set(&s_alert, items, n);
}

const jdm_page_t page_triple = {"triple", create, update, 33};
