// ================================================================
//  page_analog.c — 机械压力表(原型 12 页 PRESSURE / ANALOG)
//
//  41 根刻度的 270° 针式表盘(量程 −1..3 bar),无 header;
//  BOOST 标识融进表盘内部,读数与单位居中在下。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
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

static void create(lv_obj_t *dial)
{
    // 41 根刻度:major(i%10)r250 w5 / mid(i%5)r260 w3 / minor r269 w2;
    // i≥35(≈2.3bar 以上)红,mid 暖白,其余 line 色
    for (int i = 0; i <= 40; i++) {
        int deg = 135 + i * 270 / 40;
        bool major = (i % 10 == 0), mid = (i % 5 == 0);
        int32_t r2 = major ? 250 : mid ? 260 : 269;
        int32_t w = major ? 5 : mid ? 3 : 2;
        jdm_color_role_t c = (i >= 35) ? JDM_ROLE_RED : mid ? JDM_ROLE_TEXT : JDM_ROLE_LINE;
        jdm_draw_tick(dial, 360, 360, 280, r2, deg, w, c);
    }
    // 数值标签 −1..3(内圈 r218,文字顶边 y−14)
    for (int v = -1; v <= 3; v++) {
        jdm_pt_t p = jdm_draw_polar(360, 360, 218, 135 + (v + 1) * 270 / 4);
        char txt[4];
        snprintf(txt, sizeof(txt), "%d", v);
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

    s_value = jdm_text(dial, 360, 466, "--", JDM_FONT_ANALOG, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    jdm_text(dial, 360, 579, "bar", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    jdm_alert_widgets_t alert;   // 页脚摘要占位(M2 暂无通道报警规则)
    jdm_alert_create(dial, 0, &alert);
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
        // 量程 −1..3 bar:raw 0.1bar → bar×10;角度 = 135+(bar+1)/4×270
        int deg = 135 + (raw + 10) * 270 / 40;   // raw 0.1bar,量程 −1..3 bar
        jdm_needle_set_angle(s_needle, deg);
    }
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, raw, conn && valid);
    lv_label_set_text(s_value, buf);
}

const jdm_page_t page_analog = {"analog", create, update, 33};
