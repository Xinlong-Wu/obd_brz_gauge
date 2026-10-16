// ================================================================
//  page_pair.c — 配对码(原型 16 页 PAIR DEVICE)
//
//  蓝牙图标 + 配对码(PAIR 字号,由设备 MAC 后 4 字节派生)+
//  确认 / 返回。配对码显示为 "1234 5678" 形态。
// ================================================================

#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "esp_mac.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_header.h"

#include <stdio.h>

static void back_cb(lv_event_t *e)
{
    (void)e;
    jdm_nav_step(-1);
}

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "PAIR DEVICE", "BLUETOOTH");

    // 蓝牙图标(线稿,blue):原型 icon(332,184,'M15 0V72L44 51L0 15M0 57L44 21L15 0',
    // 'blue',1.1),线宽 2.5×1.1≈3。两条子路径——X 交叉在中央,右缘双峰
    // (380,207)/(380,240),左端开口 (332,200)/(332,247) 之间无竖线
    static const jdm_pt_t icon_a[] = {
        {348, 184}, {348, 263}, {380, 240}, {332, 200},
    };
    static const jdm_pt_t icon_b[] = {
        {332, 247}, {380, 207}, {348, 184},
    };
    jdm_draw_poly(page, icon_a, 4, 3, JDM_ROLE_BLUE);
    jdm_draw_poly(page, icon_b, 3, 3, JDM_ROLE_BLUE);

    jdm_text(page, 360, 278, "PAIRING CODE", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    // 配对码:MAC 后 4 字节 → 8 位数字,"XXXX XXXX"
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char code[16];
    snprintf(code, sizeof(code), "%02X%02X %02X%02X", mac[2], mac[3], mac[4], mac[5]);
    jdm_text(page, 360, 318 + 8 - jdm_text_glyph_top_off(jdm_font(JDM_FONT_PAIR)), code, JDM_FONT_PAIR, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);

    jdm_text(page, 360, 402, "Enter this code on your phone",
             JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    lv_obj_t *confirm = jdm_btn_create(page, 190, 440, 340, "CONFIRM PAIRING", JDM_BTN_PRIMARY);
    lv_obj_add_event_cb(confirm, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back = jdm_btn_create_ex(page, 248, 552, 224, "BACK", JDM_BTN_GHOST, true);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
}

static void update(void)
{
}

const jdm_page_t page_pair = {"pair", create, update, 200};
