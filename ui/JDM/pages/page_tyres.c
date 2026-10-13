// ================================================================
//  page_tyres.c — 四轮胎压 / 胎温(原型 08 页 TYRE MONITOR)
//
//  敞篷跑车俯视线稿(贝塞尔已展平为折线)+ 四轮数据组;
//  ZC6 胎压通道(0.1 bar)。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout_logic.h"

#include <stdio.h>

// roadster 线稿(展平贝塞尔,生成自 art.js)
static const jdm_pt_t s_body_pts[] = {
    {328, 197}, {322, 202}, {317, 207}, {312, 212}, {308, 218}, {304, 224},
    {300, 230}, {297, 237}, {294, 244}, {292, 251}, {290, 259}, {288, 267},
    {287, 275}, {286, 284}, {285, 293}, {285, 460}, {285, 466}, {286, 472},
    {288, 478}, {289, 484}, {291, 490}, {294, 495}, {297, 501}, {300, 506},
    {304, 511}, {308, 515}, {312, 519}, {316, 523}, {321, 527}, {326, 530},
    {394, 530}, {399, 527}, {404, 523}, {408, 519}, {412, 515}, {416, 511},
    {420, 506}, {423, 501}, {426, 495}, {429, 490}, {431, 484}, {432, 478},
    {434, 472}, {435, 466}, {435, 460}, {435, 293}, {434, 284}, {433, 275},
    {432, 267}, {430, 259}, {428, 251}, {426, 244}, {423, 237}, {420, 230},
    {416, 224}, {412, 218}, {408, 212}, {403, 207}, {398, 202}, {392, 197},
    {328, 197},
};
static const jdm_pt_t s_ws_pts[] = {
    {300, 275}, {307, 272}, {314, 269}, {322, 266}, {331, 265}, {340, 263},
    {350, 263}, {360, 262}, {370, 263}, {380, 263}, {389, 265}, {398, 266},
    {406, 269}, {413, 272}, {420, 275}, {408, 311}, {312, 311}, {300, 275},
};

static struct {
    lv_obj_t *name;
    lv_obj_t *pressure;
    lv_obj_t *temp;
    lv_obj_t *warn;         // 异常警示横线(y+143,116×3)
    disp_item_t ch;
    disp_item_t tch;
} s_wheel[4];
static tyre_cfg_t s_tyre;   // 当前车型的胎压/胎温报警阈值(NVS 车型选择)

static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "TYRE MONITOR", "MX-5 / ROADSTER");

    // 车身(轮廓)+ 座舱 + 座椅 + 轴线 + 轮位 + 车灯
    jdm_draw_poly(page, s_body_pts, (int)(sizeof(s_body_pts) / sizeof(s_body_pts[0])), 3, JDM_ROLE_TEXT);
    jdm_draw_poly(page, s_ws_pts, (int)(sizeof(s_ws_pts) / sizeof(s_ws_pts[0])), 2, JDM_ROLE_MUTED);
    static const jdm_pt_t seat1[] = {{323, 332}, {348, 332}, {348, 384}, {323, 384}, {323, 332}};
    static const jdm_pt_t seat2[] = {{372, 332}, {397, 332}, {397, 384}, {372, 384}, {372, 332}};
    jdm_draw_poly(page, seat1, 5, 3, JDM_ROLE_AMBER);
    jdm_draw_poly(page, seat2, 5, 3, JDM_ROLE_AMBER);
    static const jdm_pt_t belt[] = {{301, 425}, {419, 425}};
    jdm_draw_poly(page, belt, 2, 2, JDM_ROLE_MUTED);
    static const jdm_pt_t belt2[] = {{315, 502}, {405, 502}};
    jdm_draw_poly(page, belt2, 2, 2, JDM_ROLE_MUTED);
    // 四轮短轴(粗圆帽线)
    static const struct { int32_t x; int32_t y1, y2; } stubs[4] = {
        {277, 262, 304}, {443, 262, 304}, {277, 435, 477}, {443, 435, 477},
    };
    for (int i = 0; i < 4; i++) {
        jdm_pt_t s[2] = {{stubs[i].x, stubs[i].y1}, {stubs[i].x, stubs[i].y2}};
        jdm_draw_poly(page, s, 2, 10, JDM_ROLE_MUTED);
        jdm_pt_t t1[2] = {{stubs[i].x - 50, (stubs[i].y1 + stubs[i].y2) / 2}, {stubs[i].x - 3, (stubs[i].y1 + stubs[i].y2) / 2}};
        jdm_draw_poly(page, t1, 2, 2, JDM_ROLE_LINE);
        jdm_pt_t t2[2] = {{stubs[i].x + 3, (stubs[i].y1 + stubs[i].y2) / 2}, {stubs[i].x + 50, (stubs[i].y1 + stubs[i].y2) / 2}};
        jdm_draw_poly(page, t2, 2, 2, JDM_ROLE_LINE);
    }
    static const jdm_pt_t lamp[] = {{343, 218}, {351, 218}};
    jdm_draw_poly(page, lamp, 2, 4, JDM_ROLE_AMBER);
    static const jdm_pt_t lamp2[] = {{369, 218}, {377, 218}};
    jdm_draw_poly(page, lamp2, 2, 4, JDM_ROLE_AMBER);

    // 四轮数据组:FL/FR/RL/RR
    static const struct { int32_t x, y; const char *name; disp_item_t ch; disp_item_t tch; } wheels[4] = {
        {186, 252, "FL", DISP_ITEM_TPMS_FL, DISP_ITEM_TT_FL},
        {534, 252, "FR", DISP_ITEM_TPMS_FR, DISP_ITEM_TT_FR},
        {186, 430, "RL", DISP_ITEM_TPMS_RL, DISP_ITEM_TT_RL},
        {534, 430, "RR", DISP_ITEM_TPMS_RR, DISP_ITEM_TT_RR},
    };
    s_tyre = *vehicle_profile_get_tyre_cfg();
    for (int i = 0; i < 4; i++) {
        s_wheel[i].ch = wheels[i].ch;
        s_wheel[i].tch = wheels[i].tch;
        s_wheel[i].name = jdm_text(page, wheels[i].x, wheels[i].y, wheels[i].name,
                                   JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
        s_wheel[i].pressure = jdm_text(page, wheels[i].x, wheels[i].y + 29, "--",
                                       JDM_FONT_TYRE, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
        // 胎温通道 ZC6 未提供,单位行先显示 bar(原型格式 "bar / 35°C")
        s_wheel[i].temp = jdm_text(page, wheels[i].x, wheels[i].y + 112, "bar",
                                   JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
        // 异常警示横线(原型 y+143,116×3;初始隐藏)
        jdm_pt_t seg[2] = {{wheels[i].x - 58, wheels[i].y + 143},
                           {wheels[i].x + 58, wheels[i].y + 143}};
        lv_obj_t *w = jdm_draw_poly(page, seg, 2, 3, JDM_ROLE_RED);
        lv_obj_set_hidden(w, true);
        s_wheel[i].warn = w;
    }

    jdm_alert_create(page, 610, &s_alert);
}

static void update(void)
{
    static const char *const pos[4] = {"FL", "FR", "RL", "RR"};
    static char reasons[4][24];
    const char *reason = NULL;
    jdm_alert_item_t alerts[4];
    uint8_t n = 0;
    bool conn = jdm_conn_active();
    char buf[24];

    for (int i = 0; i < 4; i++) {
        int32_t raw = 0, traw = 0;
        bool ok = jdm_data_read_cache(s_wheel[i].ch, &raw) && conn;
        bool tok = jdm_data_read_cache(s_wheel[i].tch, &traw) && conn;
        if (ok) {
            snprintf(buf, sizeof(buf), "%d.%d", (int)(raw / 10), (int)(raw % 10));
            lv_label_set_text(s_wheel[i].pressure, buf);
        } else {
            lv_label_set_text(s_wheel[i].pressure, "\xE2\x80\x94");
        }
        // 单位行:胎温有效显示 "bar / NN°C",否则只显示 bar
        if (tok) {
            snprintf(buf, sizeof(buf), "bar / %d\xC2\xB0" "C", (int)traw);
            lv_label_set_text(s_wheel[i].temp, buf);
        }

        // 阈值判定走共享逻辑(胎压:≤low critical / ≥high warning;胎温:≥temp_high)
        jdm_alert_level_t lv = JDM_ALERT_NONE;
        if (ok) lv = jdm_alerts_logic_tyre(raw, s_tyre.press_low_x10, s_tyre.press_high_x10, &reason);
        if (lv == JDM_ALERT_NONE && tok && traw >= s_tyre.temp_high_c)
            lv = jdm_alerts_logic_tyre_temp(traw, s_tyre.temp_high_c, &reason);
        // 轮胎页告警统一红色(低压/高压/胎温高);页脚文案区分具体原因
        jdm_color_role_t ink = (lv == JDM_ALERT_NONE) ? JDM_ROLE_MUTED : JDM_ROLE_RED;
        // 数值/轮位标签/警示横线统一告警色(低压红,高胎压/胎温琥珀)
        lv_obj_set_style_text_color(s_wheel[i].name, jdm_color(ink), 0);
        lv_obj_set_style_text_color(s_wheel[i].pressure, jdm_color(lv == JDM_ALERT_NONE ? JDM_ROLE_TEXT : ink), 0);
        lv_obj_set_style_line_color(s_wheel[i].warn, jdm_color(ink), 0);
        lv_obj_set_hidden(s_wheel[i].warn, lv == JDM_ALERT_NONE);

        if (lv != JDM_ALERT_NONE) {
            snprintf(reasons[i], sizeof(reasons[i]), "%s / %s", pos[i],
                     (lv == JDM_ALERT_CRITICAL) ? "PRESS LOW"
                     : (lv == JDM_ALERT_WARNING && ok) ? "PRESS HIGH" : "TEMP HIGH");
            n = jdm_alerts_logic_push(alerts, n, 4, reasons[i], lv);
        }
    }
    jdm_alert_set(&s_alert, alerts, n);
}

const jdm_page_t page_tyres = {"tyres", create, update, 200};
