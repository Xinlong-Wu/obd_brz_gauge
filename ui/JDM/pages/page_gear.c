// ================================================================
//  page_gear.c — 挡位 / 转速(原型 01 页,默认首页)
//
//  56 段转速弧 + 中央大挡位 + RPM 文字;底部油温/油压摘要。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"
#include "widgets/jdm_tach.h"

#include <stdio.h>

static lv_obj_t *s_tach;
static jdm_readout_t s_gear;
static lv_obj_t *s_rpm_text;
static lv_obj_t *s_oil_text;
static lv_obj_t *s_press_text;
static lv_obj_t *s_wait_text;
static jdm_alert_widgets_t s_alert;
static tach_cfg_t s_tach_cfg;   // 当前车型的量程/警戒区(NVS 车型选择)

static void create(lv_obj_t *page)
{
    s_tach_cfg = *vehicle_profile_get_tach_cfg();   // 车型的量程/警戒区
    s_tach = jdm_tach_create(page);
    jdm_tach_apply_zones(s_tach_cfg.rpm_max, s_tach_cfg.amber_rpm, s_tach_cfg.red_rpm);
    jdm_text(page, 360, 172, "MX-5 / NC", JDM_FONT_LABEL, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);

    jdm_readout_create(page, 360, 240, JDM_RO_PRIMARY, &s_gear);
    jdm_readout_set_label(&s_gear, "GEAR");

    s_rpm_text = jdm_text(page, 360, 472, "RPM", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    s_oil_text = jdm_text(page, 256, 535, "", JDM_FONT_SUPPORT, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_press_text = jdm_text(page, 466, 535, "", JDM_FONT_SUPPORT, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_wait_text = jdm_text(page, 360, 535, "WAITING FOR VEHICLE", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    lv_obj_set_hidden(s_oil_text, true);
    lv_obj_set_hidden(s_press_text, true);

    jdm_alert_create(page, 590, &s_alert);
}

static void set_texts(int32_t oil, bool oil_ok, int32_t press, bool press_ok)
{
    char buf[24];
    if (oil_ok) {
        snprintf(buf, sizeof(buf), "OIL %d\xC2\xB0" "C", (int)oil);
        lv_label_set_text(s_oil_text, buf);
        lv_obj_set_style_text_color(s_oil_text, jdm_color(oil >= 110 ? JDM_ROLE_RED : JDM_ROLE_TEXT), 0);
    } else {
        lv_obj_set_style_text_color(s_oil_text, jdm_color(JDM_ROLE_MUTED), 0);
    }
    if (press_ok) {
        snprintf(buf, sizeof(buf), "PRESS %d.%d bar", (int)(press / 10), (int)(press % 10));
        lv_label_set_text(s_press_text, buf);
        lv_obj_set_style_text_color(s_press_text, jdm_color(press < 10 ? JDM_ROLE_RED : JDM_ROLE_TEXT), 0);
    } else {
        lv_obj_set_style_text_color(s_press_text, jdm_color(JDM_ROLE_MUTED), 0);
    }
}

static void update(void)
{
    int32_t rpm = 0, oil = 0, press = 0;
    bool conn = jdm_conn_active();
    bool rpm_ok = jdm_data_read_cache(DISP_ITEM_RPM, &rpm);
    bool oil_ok = jdm_data_read_cache(DISP_ITEM_OIL, &oil);
    bool press_ok = jdm_data_read_cache(DISP_ITEM_OILP, &press);

    jdm_tach_update(s_tach, rpm, conn && rpm_ok);

    // 挡位:数据层 gear(127=无效);断连显示 —
    char gear_txt[4] = "\xE2\x80\x94";
    if (conn) {
        int8_t gear = obd_data_get_gear();
        if (gear != 127 && gear > 0) {
            snprintf(gear_txt, sizeof(gear_txt), "%d", (int)gear);
        } else {
            snprintf(gear_txt, sizeof(gear_txt), "N");
        }
    }
    jdm_readout_set_value(&s_gear, gear_txt);

    char buf[24];
    if (conn && rpm_ok) {
        // 千分位(原型 toLocaleString('en-US'),如 4,800 RPM)
        int v = (int)rpm;
        if (v >= 1000 && v < 1000000) {
            snprintf(buf, sizeof(buf), "%d,%03d RPM", v / 1000, v % 1000);
        } else {
            snprintf(buf, sizeof(buf), "%d RPM", v);
        }
        lv_label_set_text(s_rpm_text, buf);
    } else {
        lv_label_set_text(s_rpm_text, "RPM");
    }

    lv_obj_set_hidden(s_wait_text, conn);
    lv_obj_set_hidden(s_oil_text, !conn);
    lv_obj_set_hidden(s_press_text, !conn);
    if (conn) {
        set_texts(oil, oil_ok, press, press_ok);
    }

    // 报警:转速 / 油温 / 油压(critical 优先;断连抑制)
    jdm_alert_item_t items[3];
    int n = 0;
    const char *reason = NULL;
    if (conn && rpm_ok) {
        jdm_alert_level_t lv = jdm_alerts_logic_rpm(rpm, s_tach_cfg.shift_rpm,
                                                    s_tach_cfg.red_rpm, &reason);
        n = jdm_alerts_logic_push(items, n, 3, reason, lv);
        lv_obj_set_style_text_color(s_rpm_text,
                                    jdm_color(lv == JDM_ALERT_NONE ? JDM_ROLE_MUTED
                                              : lv == JDM_ALERT_ADVISORY ? JDM_ROLE_AMBER
                                              : JDM_ROLE_RED), 0);
    } else {
        lv_obj_set_style_text_color(s_rpm_text, jdm_color(JDM_ROLE_MUTED), 0);
    }
    if (conn && oil_ok) {
        jdm_alert_level_t lv = jdm_alerts_logic_oil(oil, &reason);
        n = jdm_alerts_logic_push(items, n, 3, reason, lv);
    }
    if (conn && press_ok) {
        jdm_alert_level_t lv = jdm_alerts_logic_oilp(press, &reason);
        n = jdm_alerts_logic_push(items, n, 3, reason, lv);
    }
    jdm_alert_set(&s_alert, items, n);
}

const jdm_page_t page_gear = {"gear", create, update, 33};
