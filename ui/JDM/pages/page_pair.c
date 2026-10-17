// ================================================================
//  page_pair.c — 手机配对(原型 16 页 PHONE PAIRING)
//
//  绑定持久化:NVS ble_phone_mac(非零 = 已配对)。CONFIRM PAIRING 在
//  手机 GATT(RaceChrono 服务)已连接时把该手机写入 NVS;FORGET /
//  PAIR NEW 真实清除绑定。配对码为 MAC 派生显示(App 侧关联用)。
//  DISCOVERABLE 开关为 UI 预留(固件暂无可发现性能力)。BACK 回连接
//  总览页(14)。
// ================================================================

#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/racechrono_ble_diy.h"
#include "esp_mac.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_toggle.h"

#include <stdio.h>
#include <string.h>

#define PAGE_BLUETOOTH  14      // 连接总览(见 jdm_ui.c 页表)

static lv_obj_t *s_norm;        // 普通态容器
static lv_obj_t *s_done;        // 完成态容器
static lv_obj_t *s_toggle;
static lv_obj_t *s_status;      // y280:PAIRING CODE / NOT DISCOVERABLE
static lv_obj_t *s_code;        // y320:配对码
static lv_obj_t *s_hint;        // y408:提示
static lv_obj_t *s_confirm;
static lv_obj_t *s_done_sub;    // 完成态副行(连接中/待回连文案)
static bool s_visible = true;
static bool s_dialog;           // 忘记确认对话框
static lv_obj_t *s_dialog_obj;
static char s_code_text_buf[16];        // MAC 派生配对码("XXXX XXXX")

static const char *s_code_text(void)
{
    return s_code_text_buf;
}

static bool phone_paired_nv(void)
{
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    return cfg->ble_phone_mac[0] | cfg->ble_phone_mac[1] | cfg->ble_phone_mac[2] |
           cfg->ble_phone_mac[3] | cfg->ble_phone_mac[4] | cfg->ble_phone_mac[5];
}

static void phone_forget_nv(void)
{
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memset(cfg->ble_phone_mac, 0, sizeof(cfg->ble_phone_mac));
    nvs_cfg_set(cfg);
}

static void phone_bind_nv(const uint8_t mac[6])
{
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memcpy(cfg->ble_phone_mac, mac, sizeof(cfg->ble_phone_mac));
    nvs_cfg_set(cfg);
}

static void refresh(void)
{
    bool paired = phone_paired_nv();
    bool live = racechrono_ble_diy_get_phone(NULL);

    lv_obj_set_hidden(s_norm, paired);
    lv_obj_set_hidden(s_done, !paired);

    if (paired) {
        lv_label_set_text(s_done_sub, live ? "App connection is ready"
                                           : "Open the app to reconnect");
    } else {
        jdm_toggle_set(s_toggle, s_visible);
        lv_label_set_text(s_status, s_visible ? "PAIRING CODE" : "NOT DISCOVERABLE");
        lv_label_set_text(s_code, s_visible ? s_code_text() : "\xE2\x80\x94\x20\xE2\x80\x94");
        const char *hint = !s_visible ? "Turn on discoverable to pair"
                          : live      ? "Enter this code in the phone app"
                                      : "Connect the SkyGauge app to pair";
        lv_label_set_text(s_hint, hint);
        jdm_btn_set_disabled(s_confirm, !s_visible || !live);
    }
}

// ---- 动作 ----

static void visible_cb(lv_event_t *e)
{
    (void)e;
    s_visible = !s_visible;
    refresh();
}

static void confirm_cb(lv_event_t *e)
{
    (void)e;
    uint8_t mac[6];
    if (racechrono_ble_diy_get_phone(mac)) {
        phone_bind_nv(mac);     // 持久绑定当前 GATT 手机
    }
    refresh();
}

static void new_pair_cb(lv_event_t *e)
{
    (void)e;
    phone_forget_nv();          // 清绑定重新配对
    s_visible = true;
    refresh();
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    jdm_nav_goto(PAGE_BLUETOOTH);
}

// ---- 忘记确认对话框 ----

static void dialog_close(void)
{
    if (s_dialog_obj) {
        lv_obj_delete(s_dialog_obj);
        s_dialog_obj = NULL;
    }
    s_dialog = false;
}

static void dialog_cancel_cb(lv_event_t *e)
{
    (void)e;
    dialog_close();
}

static void dialog_forget_cb(lv_event_t *e)
{
    (void)e;
    phone_forget_nv();
    dialog_close();
    refresh();
}

static void forget_cb(lv_event_t *e)
{
    (void)e;
    if (s_dialog) return;       // 防重入
    s_dialog = true;

    s_dialog_obj = lv_obj_create(lv_obj_get_parent(s_done));
    lv_obj_remove_style_all(s_dialog_obj);
    lv_obj_set_size(s_dialog_obj, UIS(720), UIS(720));
    lv_obj_set_pos(s_dialog_obj, 0, 0);
    lv_obj_clear_flag(s_dialog_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_dialog_obj, jdm_color(JDM_ROLE_FACE), 0);
    lv_obj_set_style_bg_opa(s_dialog_obj, 240, 0);

    lv_obj_t *panel = lv_obj_create(s_dialog_obj);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, UIS(114), UIS(200));
    lv_obj_set_size(panel, UIS(492), UIS(354));
    lv_obj_set_style_radius(panel, UIS(20), 0);
    lv_obj_set_style_bg_color(panel, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    jdm_text(s_dialog_obj, 360, 240, "FORGET PHONE?", JDM_FONT_BODY, JDM_ROLE_RED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog_obj, 360, 299, "ROADSTER PHONE", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog_obj, 360, 350, "OBD connection is kept.", JDM_FONT_SMALL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    lv_obj_t *cancel = jdm_btn_create(s_dialog_obj, 148, 434, 192, "CANCEL", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(cancel, dialog_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *forget = jdm_btn_create(s_dialog_obj, 380, 434, 192, "FORGET", JDM_BTN_DANGER);
    lv_obj_add_event_cb(forget, dialog_forget_cb, LV_EVENT_CLICKED, NULL);
}

// ---- 构建 ----

static void build_normal(lv_obj_t *page)
{
    s_norm = lv_obj_create(page);
    lv_obj_remove_style_all(s_norm);
    lv_obj_set_size(s_norm, UIS(720), UIS(720));
    lv_obj_set_pos(s_norm, 0, 0);
    lv_obj_clear_flag(s_norm, LV_OBJ_FLAG_SCROLLABLE);

    jdm_text(s_norm, 158, 186, "DISCOVERABLE", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);
    s_toggle = jdm_toggle_create(s_norm, 474, 166, 104, s_visible);
    lv_obj_add_event_cb(s_toggle, visible_cb, LV_EVENT_CLICKED, NULL);

    s_status = jdm_text(s_norm, 360, 280, "PAIRING CODE", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    s_code = jdm_text(s_norm, 360, 318 + 8 - jdm_text_glyph_top_off(jdm_font(JDM_FONT_PAIR)),
                      "", JDM_FONT_PAIR, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_hint = jdm_text(s_norm, 360, 408, "Connect the SkyGauge app to pair",
                      JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    s_confirm = jdm_btn_create(s_norm, 190, 450, 340, "CONFIRM PAIRING", JDM_BTN_PRIMARY);
    lv_obj_add_event_cb(s_confirm, confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = jdm_btn_create_ex(s_norm, 248, 552, 224, "BACK", JDM_BTN_GHOST, true);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
}

static void build_done(lv_obj_t *page)
{
    s_done = lv_obj_create(page);
    lv_obj_remove_style_all(s_done);
    lv_obj_set_size(s_done, UIS(720), UIS(720));
    lv_obj_set_pos(s_done, 0, 0);
    lv_obj_clear_flag(s_done, LV_OBJ_FLAG_SCROLLABLE);

    // 蓝牙图标(线稿,blue):原型 icon(332,184,'M15 0V72L44 51L0 15M0 57L44 21L15 0',1.1)
    static const jdm_pt_t icon_a[] = {
        {348, 184}, {348, 263}, {380, 240}, {332, 200},
    };
    static const jdm_pt_t icon_b[] = {
        {332, 247}, {380, 207}, {348, 184},
    };
    jdm_draw_poly(s_done, icon_a, 4, 3, JDM_ROLE_BLUE);
    jdm_draw_poly(s_done, icon_b, 3, 3, JDM_ROLE_BLUE);

    jdm_text(s_done, 360, 288, "PHONE CONNECTED", JDM_FONT_TITLE, JDM_ROLE_GREEN, JDM_TEXT_MIDDLE);
    jdm_text(s_done, 360, 336, "ROADSTER PHONE", JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_done_sub = jdm_text(s_done, 360, 382, "App connection is ready",
                          JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    lv_obj_t *pair_new = jdm_btn_create(s_done, 152, 438, 200, "PAIR NEW", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(pair_new, new_pair_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *forget = jdm_btn_create(s_done, 368, 438, 200, "FORGET", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(forget, forget_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = jdm_btn_create_ex(s_done, 248, 552, 224, "BACK", JDM_BTN_GHOST, true);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
}

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "PHONE PAIRING", "PHONE \xE2\x86\x92 GAUGE");

    build_normal(page);
    build_done(page);

    // 配对码:MAC 后 4 字节 → 8 位十六进制,"XXXX XXXX"
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_code_text_buf, sizeof(s_code_text_buf), "%02X%02X %02X%02X",
             mac[2], mac[3], mac[4], mac[5]);

    refresh();
}

static void update(void)
{
    refresh();      // 200ms 轮询:GATT 连接态/绑定变化即时反映
}

const jdm_page_t page_pair = {"pair", create, update, 200};
