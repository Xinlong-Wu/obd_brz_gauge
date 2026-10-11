// ================================================================
//  page_bluetooth.c — 蓝牙 / 遥控(原型 15 页 CONNECTIONS)
//
//  状态映射固件现实:已绑定适配器且已连接 → DEVICE CONNECTED(绿);
//  已绑定未连接 → CONNECTION LOST(琥珀,页脚 "SAVED PAIRING
//  RETAINED");未绑定 → NO DEVICE(muted)。BACK 回 01 gear 页;
//  PAIR 跳配对码页;FORGET ALL(ghost 双行)弹清除确认对话框后才
//  清适配器绑定(NVS);开关为 UI 态(遥控键盘/可发现性在固件侧
//  暂无对应能力,保留交互)。
// ================================================================

#include "core/jdm_conn.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_chip.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_toggle.h"

#include <string.h>

static lv_obj_t *s_chip;
static lv_obj_t *s_device_lbl;
static lv_obj_t *s_retained_lbl;
static lv_obj_t *s_visible_toggle;
static lv_obj_t *s_keyboard_toggle;
static lv_obj_t *s_dialog;          // 清除确认对话框(非 NULL = 打开)
static bool s_visible_on = true;
static bool s_keyboard_on = false;
static void refresh_page_state(void);

static void visible_cb(lv_event_t *e)
{
    (void)e;
    s_visible_on = !s_visible_on;
    jdm_toggle_set(s_visible_toggle, s_visible_on);
}

static void keyboard_cb(lv_event_t *e)
{
    (void)e;
    s_keyboard_on = !s_keyboard_on;
    jdm_toggle_set(s_keyboard_toggle, s_keyboard_on);
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    jdm_nav_goto(0);    // 原型 15 页 BACK 回 01 gear 页
}

static void pair_cb(lv_event_t *e)
{
    (void)e;
    // 跳到配对码页(轮播末页,见 jdm_ui.c 页表)
    jdm_nav_goto(15);
}

static void forget_apply(void)
{
    // 清适配器绑定(名字 + MAC)
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memset(cfg->ble_device_name, 0, sizeof(cfg->ble_device_name));
    memset(cfg->ble_obd_mac, 0, sizeof(cfg->ble_obd_mac));
    nvs_cfg_set(cfg);
}

static void dialog_close(void)
{
    if (s_dialog) {
        lv_obj_delete(s_dialog);
        s_dialog = NULL;
    }
}

static void cancel_cb(lv_event_t *e)
{
    (void)e;
    dialog_close();
}

static void clear_cb(lv_event_t *e)
{
    (void)e;
    forget_apply();
    dialog_close();
    refresh_page_state();
}

static void forget_cb(lv_event_t *e)
{
    (void)e;
    if (s_dialog) return;   // 防重入

    // 确认对话框(原型 15 页):遮罩 + panel 面板 + 红标题 + 两键
    s_dialog = lv_obj_create(lv_obj_get_parent(s_chip));
    lv_obj_remove_style_all(s_dialog);
    lv_obj_set_size(s_dialog, UIS(720), UIS(720));
    lv_obj_set_pos(s_dialog, 0, 0);
    lv_obj_clear_flag(s_dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_dialog, jdm_color(JDM_ROLE_FACE), 0);
    lv_obj_set_style_bg_opa(s_dialog, 240, 0);   // 原型 .94

    lv_obj_t *panel = lv_obj_create(s_dialog);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, UIS(114), UIS(200));
    lv_obj_set_size(panel, UIS(492), UIS(354));
    lv_obj_set_style_radius(panel, UIS(20), 0);
    lv_obj_set_style_bg_color(panel, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    jdm_text(s_dialog, 360, 240, "CLEAR PAIRED DEVICES?",
             JDM_FONT_BODY, JDM_ROLE_RED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog, 360, 299, "PHONE + REMOTE KEYBOARD",
             JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog, 360, 335, "Saved pairings will be",
             JDM_FONT_SMALL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog, 360, 367, "removed from this device.",
             JDM_FONT_SMALL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);

    lv_obj_t *cancel = jdm_btn_create(s_dialog, 148, 434, 192, "CANCEL", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(cancel, cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clear = jdm_btn_create(s_dialog, 380, 434, 192, "CLEAR", JDM_BTN_DANGER);
    lv_obj_add_event_cb(clear, clear_cb, LV_EVENT_CLICKED, NULL);
}

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "CONNECTIONS", "BLUETOOTH");

    s_chip = jdm_chip_create(page, 175, 176, 370, "NO DEVICE", JDM_ROLE_PANEL);
    s_device_lbl = jdm_text(page, 360, 232, "PAIR A NEW DEVICE", JDM_FONT_LABEL,
                            JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    s_retained_lbl = jdm_text(page, 360, 615, "SAVED PAIRING RETAINED",
                              JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    lv_obj_set_hidden(s_retained_lbl, true);
    static const jdm_pt_t sep[] = {{128, 270}, {592, 270}};
    jdm_draw_poly(page, sep, 2, 2, JDM_ROLE_LINE);

    jdm_text(page, 142, 318, "DISCOVERABLE", JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_START);
    s_visible_toggle = jdm_toggle_create(page, 474, 286, 104, s_visible_on);
    lv_obj_add_event_cb(s_visible_toggle, visible_cb, LV_EVENT_CLICKED, NULL);

    jdm_text(page, 142, 418, "REMOTE KEY", JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_START);
    s_keyboard_toggle = jdm_toggle_create(page, 474, 386, 104, s_keyboard_on);
    lv_obj_add_event_cb(s_keyboard_toggle, keyboard_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = jdm_btn_create_ex(page, 160, 502, 112, "BACK", JDM_BTN_GHOST, true);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pair = jdm_btn_create(page, 292, 502, 112, "PAIR", JDM_BTN_PRIMARY);
    lv_obj_add_event_cb(pair, pair_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *forget = jdm_btn_create_ex(page, 424, 502, 136, "FORGET\nALL",
                                         JDM_BTN_GHOST, false);
    lv_obj_add_event_cb(forget, forget_cb, LV_EVENT_CLICKED, NULL);
}

static void refresh_page_state(void)
{
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    bool bound = (cfg->ble_obd_mac[0] | cfg->ble_obd_mac[1] | cfg->ble_obd_mac[2] |
                  cfg->ble_obd_mac[3] | cfg->ble_obd_mac[4] | cfg->ble_obd_mac[5]) != 0;
    bool connected = jdm_conn_active();

    if (bound && connected) {
        jdm_chip_set(s_chip, "DEVICE CONNECTED", JDM_ROLE_PANEL);
        lv_label_set_text(s_device_lbl, cfg->ble_device_name[0] ? cfg->ble_device_name
                                                                : "ROADSTER ELM327");
        lv_obj_set_style_text_color(s_device_lbl, jdm_color(JDM_ROLE_GREEN), 0);
    } else if (bound) {
        jdm_chip_set(s_chip, "CONNECTION LOST", JDM_ROLE_AMBER);
        lv_label_set_text(s_device_lbl, "RECONNECT YOUR ADAPTER");
        lv_obj_set_style_text_color(s_device_lbl, jdm_color(JDM_ROLE_AMBER), 0);
    } else {
        jdm_chip_set(s_chip, "NO DEVICE", JDM_ROLE_PANEL);
        lv_label_set_text(s_device_lbl, "PAIR A NEW DEVICE");
        lv_obj_set_style_text_color(s_device_lbl, jdm_color(JDM_ROLE_MUTED), 0);
    }
    // 已绑定未连接:页脚保留已存配对提示(原型 connection-lost 态)
    lv_obj_set_hidden(s_retained_lbl, !(bound && !connected));
}

static void update(void)
{
    refresh_page_state();
}

const jdm_page_t page_bluetooth = {"bluetooth", create, update, 200};
