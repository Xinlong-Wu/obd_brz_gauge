// ================================================================
//  page_multi.c — 多项综合监测(原型 07 页 ENGINE VITALS)
//
//  十字分隔 + 四角 compact 读数(油压/油温/电压/水温);
//  中央 TPS / B.P 切换区(点击切换,琥珀下划线)。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"

#include <stdio.h>
#include <string.h>

static jdm_readout_t s_press_ro;
static jdm_readout_t s_oil_ro;
static jdm_readout_t s_bat_ro;
static jdm_readout_t s_clt_ro;
static lv_obj_t *s_boost_big;
static lv_obj_t *s_boost_lbl;
static lv_obj_t *s_tps_lbl;
static lv_obj_t *s_bp_lbl;
static lv_obj_t *s_switch_val;
static lv_obj_t *s_under_tps;
static lv_obj_t *s_under_bp;
static bool s_show_tps = true;
static void switch_tap_cb(lv_event_t *e);
static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "ENGINE VITALS", NULL);

    // 十字分隔线
    jdm_pt_t h1[2] = {{108, 314}, {612, 314}};
    jdm_draw_poly(page, h1, 2, 2, JDM_ROLE_LINE);
    jdm_pt_t v1[2] = {{360, 162}, {360, 308}};
    jdm_draw_poly(page, v1, 2, 2, JDM_ROLE_LINE);
    jdm_pt_t h2[2] = {{108, 466}, {612, 466}};
    jdm_draw_poly(page, h2, 2, 2, JDM_ROLE_LINE);
    jdm_pt_t v2[2] = {{360, 470}, {360, 610}};
    jdm_draw_poly(page, v2, 2, 2, JDM_ROLE_LINE);

    // 四角 compact(cx = x+115)
    jdm_readout_create(page, 230, 166, JDM_RO_COMPACT, &s_press_ro);
    jdm_readout_set_label(&s_press_ro, "OIL PRESS");
    jdm_readout_set_unit(&s_press_ro, "bar");
    jdm_readout_create(page, 490, 166, JDM_RO_COMPACT, &s_oil_ro);
    jdm_readout_set_label(&s_oil_ro, "OIL TEMP");
    jdm_readout_set_unit(&s_oil_ro, "\xC2\xB0" "C");
    jdm_readout_create(page, 230, 477, JDM_RO_COMPACT, &s_bat_ro);
    jdm_readout_set_label(&s_bat_ro, "BATTERY");
    jdm_readout_set_unit(&s_bat_ro, "V");
    jdm_readout_create(page, 490, 477, JDM_RO_COMPACT, &s_clt_ro);
    jdm_readout_set_label(&s_clt_ro, "COOLANT");
    jdm_readout_set_unit(&s_clt_ro, "\xC2\xB0" "C");

    // 左中:BOOST 大值 + 标签
    s_boost_big = jdm_text(page, 206, 336, "--", JDM_FONT_BOOST, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    s_boost_lbl = jdm_text(page, 206, 427, "BOOST / bar", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    // 右中:TPS / B.P 切换区(整体可点)
    s_tps_lbl = jdm_text(page, 450, 328, "TPS", JDM_FONT_SUPPORT, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    jdm_text(page, 490, 328, "/", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    s_bp_lbl = jdm_text(page, 530, 328, "B.P", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    // 选中下划线(x±22,y358,w3)
    s_under_tps = lv_obj_create(page);
    lv_obj_remove_style_all(s_under_tps);
    lv_obj_set_pos(s_under_tps, UIS(428), UIS(358));
    lv_obj_set_size(s_under_tps, UIS(44), UIS(3));
    lv_obj_set_style_bg_color(s_under_tps, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(s_under_tps, LV_OPA_COVER, 0);
    s_under_bp = lv_obj_create(page);
    lv_obj_remove_style_all(s_under_bp);
    lv_obj_set_pos(s_under_bp, UIS(508), UIS(358));
    lv_obj_set_size(s_under_bp, UIS(44), UIS(3));
    lv_obj_set_style_bg_color(s_under_bp, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(s_under_bp, LV_OPA_COVER, 0);

    // 切换区大值(490,378,compact 字号)
    s_switch_val = jdm_text(page, 490, 378, "--", JDM_FONT_COMPACT, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);

    // 命中区(375,318,230×140)
    lv_obj_t *hit = lv_obj_create(page);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, UIS(375), UIS(318));
    lv_obj_set_size(hit, UIS(230), UIS(140));
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, switch_tap_cb, LV_EVENT_CLICKED, NULL);

    jdm_alert_create(page, 0, &s_alert);
}

static void switch_tap_cb(lv_event_t *e)
{
    (void)e;
    s_show_tps = !s_show_tps;
}

static void apply_switch(void)
{
    lv_obj_set_style_text_color(s_tps_lbl, jdm_color(s_show_tps ? JDM_ROLE_AMBER : JDM_ROLE_MUTED), 0);
    lv_obj_set_style_text_color(s_bp_lbl, jdm_color(!s_show_tps ? JDM_ROLE_AMBER : JDM_ROLE_MUTED), 0);
    lv_obj_set_hidden(s_under_tps, !s_show_tps);
    lv_obj_set_hidden(s_under_bp, s_show_tps);
}

static void update(void)
{
    int32_t press = 0, oil = 0, bat = 0, clt = 0, tps = 0, boost = 0;
    bool conn = jdm_conn_active();
    bool press_ok = jdm_data_read_cache(DISP_ITEM_OILP, &press) && conn;
    bool oil_ok = jdm_data_read_cache(DISP_ITEM_OIL, &oil) && conn;
    bool bat_ok = jdm_data_read_cache(DISP_ITEM_BAT, &bat) && conn;
    bool clt_ok = jdm_data_read_cache(DISP_ITEM_CLT, &clt) && conn;
    bool tps_ok = jdm_data_read_cache(DISP_ITEM_TPS, &tps) && conn;
    bool boost_ok = jdm_data_read_cache(DISP_ITEM_BOOST, &boost) && conn;

    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OILP, press, press_ok);
    jdm_readout_set_value(&s_press_ro, buf);
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OIL, oil, oil_ok);
    jdm_readout_set_value(&s_oil_ro, buf);
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BAT, bat, bat_ok);
    jdm_readout_set_value(&s_bat_ro, buf);
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_CLT, clt, clt_ok);
    jdm_readout_set_value(&s_clt_ro, buf);

    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, boost, boost_ok);
    lv_label_set_text(s_boost_big, buf);

    // 报警:油压 / 油温 / 水温 / 电压 / 增压(critical 优先;断连抑制)
    jdm_alert_item_t items[5];
    int n = 0;
    const char *reason = NULL;
    jdm_alert_level_t lv;
    lv = press_ok ? jdm_alerts_logic_oilp(press, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 5, reason, lv);
    jdm_readout_set_warning(&s_press_ro, lv != JDM_ALERT_NONE);
    lv = oil_ok ? jdm_alerts_logic_oil(oil, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 5, reason, lv);
    jdm_readout_set_warning(&s_oil_ro, lv != JDM_ALERT_NONE);
    lv = clt_ok ? jdm_alerts_logic_clt(clt, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 5, reason, lv);
    jdm_readout_set_warning(&s_clt_ro, lv != JDM_ALERT_NONE);
    lv = bat_ok ? jdm_alerts_logic_bat(bat, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 5, reason, lv);
    jdm_readout_set_warning(&s_bat_ro, lv != JDM_ALERT_NONE);
    lv = boost_ok ? jdm_alerts_logic_boost(boost, &reason) : JDM_ALERT_NONE;
    n = jdm_alerts_logic_push(items, n, 5, reason, lv);
    jdm_alert_set(&s_alert, items, n);

    // 左中 BOOST 大值:报警变红,标签换 "BOOST HIGH"(原型 ink/文案切换)
    jdm_alert_level_t boost_lv = lv;
    lv_obj_set_style_text_color(s_boost_big,
                                jdm_color(boost_lv != JDM_ALERT_NONE ? JDM_ROLE_RED
                                                                       : JDM_ROLE_AMBER), 0);
    lv_label_set_text(s_boost_lbl,
                      boost_lv != JDM_ALERT_NONE ? "BOOST HIGH" : "BOOST / bar");

    // 切换区:TPS → "38%",B.P → boost 两位小数
    if (s_show_tps) {
        if (tps_ok) {
            snprintf(buf, sizeof(buf), "%d%%", (int)tps);
        } else {
            snprintf(buf, sizeof(buf), "\xE2\x80\x94");
        }
    } else {
        jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, boost, boost_ok);
    }
    lv_label_set_text(s_switch_val, buf);
    apply_switch();
}

const jdm_page_t page_multi = {"multi", create, update, 33};
