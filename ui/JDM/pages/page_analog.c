// ================================================================
//  page_analog.c — 机械压力表(原型 12 页 PRESSURE / ANALOG)
//
//  41 根刻度的 270° 针式表盘(量程 −1..3 bar),无 header;
//  BOOST 标识融进表盘内部,读数与单位居中在下。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "core/jdm_data_logic.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_needle.h"

#include <stdio.h>

#include "core/jdm_nav.h"
#include "widgets/jdm_readout_logic.h"

static lv_obj_t *s_needle;
static lv_obj_t *s_hub;
static lv_obj_t *s_value;
static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *dial)
{
    // 车型的量程/红区阈值(与 boost 页同源)
    const boost_cfg_t *bc = vehicle_profile_get_boost_cfg();
    int32_t v_lo = bc->min_x10, v_hi = bc->max_x10;
    int32_t v_range = v_hi - v_lo;

    // 41 根刻度:major(i%10)r250 w5 / mid(i%5)r260 w3 / minor r269 w2;
    // 段起始压力 ≥ 红区阈值(红),mid 暖白,其余 line 色
    for (int i = 0; i <= 40; i++) {
        float deg = 135.0f + i * 270.0f / 40.0f;
        bool major = (i % 10 == 0), mid = (i % 5 == 0);
        int32_t r2 = major ? 250 : mid ? 260 : 269;
        int32_t w = major ? 5 : mid ? 3 : 2;
        int32_t v = v_lo + (int32_t)((int64_t)i * v_range / 40);
        jdm_color_role_t c = (v >= bc->red_x10) ? JDM_ROLE_RED
                           : mid ? JDM_ROLE_TEXT : JDM_ROLE_LINE;
        jdm_draw_tick(dial, 360, 360, 280, r2, deg, w, c);
    }
    // 数值标签:整 bar 刻度由量程推导(内圈 r218,文字顶边 y−14)
    for (int32_t v10 = v_lo; v10 <= v_hi; v10 += 10) {
        float deg = 135.0f + (v10 - v_lo) * 270.0f / v_range;
        jdm_pt_t p = jdm_draw_polar(360, 360, 218, deg);
        char txt[8];
        if (v10 < 0) {
            snprintf(txt, sizeof(txt), "\xE2\x88\x92" "%d", -v10 / 10);
        } else {
            snprintf(txt, sizeof(txt), "%d", v10 / 10);
        }
        jdm_text(dial, p.x, p.y - 14, txt, JDM_FONT_LABEL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    jdm_text(dial, 360, 186, "BOOST", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);

    // 指针(在线)/ 轴心(断连)
    s_needle = jdm_needle_create_group(dial, 360, 360, 220, JDM_ROLE_AMBER);
    s_hub = lv_obj_create(dial);
    lv_obj_remove_style_all(s_hub);
    lv_obj_set_size(s_hub, UIS(18), UIS(18));
    lv_obj_set_pos(s_hub, UIS(360) - UIS(9), UIS(360) - UIS(9));
    lv_obj_set_style_radius(s_hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_hub, jdm_color(JDM_ROLE_LINE), 0);
    lv_obj_set_style_bg_opa(s_hub, LV_OPA_COVER, 0);
    lv_obj_set_hidden(s_hub, true);

    s_value = jdm_text(dial, 360, 466 + 10 - jdm_text_glyph_top_off(jdm_font(JDM_FONT_ANALOG)), "--", JDM_FONT_ANALOG, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    jdm_text(dial, 360, 579, "bar", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    jdm_alert_create(dial, 0, &s_alert);
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_BOOST, &raw);
    bool conn = jdm_conn_active();

    if (!conn) {
        lv_obj_set_hidden(s_needle, true);
        lv_obj_set_hidden(s_hub, false);
    } else if (valid) {
        lv_obj_set_hidden(s_needle, false);
        lv_obj_set_hidden(s_hub, true);
        // 角度 = 135+(raw−min)*270/量程跨度(浮点,0.1bar 步进精确)
        float deg = 135.0f + (raw - vehicle_profile_get_boost_cfg()->min_x10)
                    * 270.0f / (vehicle_profile_get_boost_cfg()->max_x10
                                - vehicle_profile_get_boost_cfg()->min_x10);
        jdm_needle_set_angle(s_needle, deg);
    }
    char buf[16];
    bool valid_conn = conn && valid;
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, raw, valid_conn);
    lv_label_set_text(s_value, buf);

    jdm_alert_item_t item;
    int n = 0;
    const char *reason = NULL;
    if (valid_conn) {
        jdm_alert_level_t lv = jdm_alerts_logic_boost(raw, vehicle_profile_get_boost_cfg()->red_x10, &reason);
        n = jdm_alerts_logic_push(&item, n, 1, reason, lv);
    }
    lv_obj_set_style_text_color(s_value,
                                jdm_color(n > 0 ? JDM_ROLE_RED : JDM_ROLE_TEXT), 0);
    jdm_alert_set(&s_alert, &item, n);
}

const jdm_page_t page_analog = {"analog", create, update, 33};
