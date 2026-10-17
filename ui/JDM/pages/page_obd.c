// ================================================================
//  page_obd.c — OBD 设备(原型 17 页 OBD SETUP)
//
//  真实数据流:RESCAN/进入列表起 elm327 BLE 真扫描(5s,全部设备按
//  信号强度排序,列表可上下滑动);点选行 → elm327_connect_by_addr
//  真连接,进度由真实事件驱动(BLE 链路建立 → READING VEHICLE,数据
//  就绪 → DATA READY 并写入 NVS 绑定);超时/连不上 → CONNECTION
//  LOST(RETRY)。FORGET 真实清除 NVS 绑定并断开。BACK/CANCEL 回连
//  接总览页(14)。
// ================================================================

#include "widgets/jdm_chip.h"
#include "bsp_obd_dsp/elm327_ble_client.h"
#include "core/jdm_conn.h"
#include "core/jdm_draw.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_header.h"

#include <stdio.h>
#include <string.h>

#define PAGE_BLUETOOTH   14     // 连接总览(见 jdm_ui.c 页表)
#define SCAN_DURATION_S  5      // 单轮扫描时长(elm327 客户端上限 20 台)
#define SCAN_SETTLE_MS   1500   // 扫描结束后等尾包再排序出表
#define CONNECT_TIMEOUT_MS 15000

typedef enum {
    SC_LIST,                    // 扫描列表
    SC_PROGRESS,                // 连接进度(s_step 1=CONNECTING 2=READING)
    SC_CONNECTED,               // DATA READY
    SC_DISCONNECTED,            // CONNECTION LOST
} obd_scene_t;

static obd_scene_t s_scene;
static int s_step;
static lv_timer_t *s_timer;         // 扫描收尾(排序出表)
static uint32_t s_deadline;         // 连接超时(lv_tick)
static bool s_dialog;
static lv_obj_t *s_dialog_obj;

// 扫描结果快照(settle 时从 elm327 客户端拷入,仅 LVGL 线程访问)
static ble_scan_result_t s_devs[BLE_SCAN_MAX_DEVICES];
static int s_dev_count;
static bool s_scanning;

static char s_sel_name[32];         // 正在连接/已绑定的设备
static uint8_t s_sel_mac[6];

static lv_obj_t *s_list;
static lv_obj_t *s_scan_hint;       // SEARCHING… n devices
static lv_obj_t *s_rows_box;        // 可滚动行容器
static lv_obj_t *s_rows[BLE_SCAN_MAX_DEVICES];
static lv_obj_t *s_progress;
static lv_obj_t *s_step_lbl;
static lv_obj_t *s_step_sub;
static lv_obj_t *s_step_hint;
static lv_obj_t *s_dots[3];
static lv_obj_t *s_prog_btn;
static lv_obj_t *s_result;
static lv_obj_t *s_chip;
static lv_obj_t *s_res_dev;
static lv_obj_t *s_res_line1;
static lv_obj_t *s_res_line2;
static lv_obj_t *s_res_change;
static lv_obj_t *s_res_forget;
static lv_obj_t *s_res_retry;

static void scene_apply(void);
static void scan_start(void);
static void row_cb(lv_event_t *e);

// 扫描收尾(LVGL 线程):停扫、按信号强度排序、重建行
static void scan_settle_cb(lv_timer_t *t)
{
    (void)t;
    fprintf(stderr, "[sim] scan settle: %d devices\n", s_dev_count);
    s_timer = NULL;
    elm327_ble_scan_only_stop();
    int n = elm327_ble_scan_results_get(s_devs);
    ble_scan_result_t tmp[BLE_SCAN_MAX_DEVICES];
    memcpy(tmp, s_devs, sizeof(ble_scan_result_t) * n);

    // RSSI 降序(简单插入排序,n≤20)
    for (int i = 1; i < n; i++) {
        ble_scan_result_t v = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j].rssi < v.rssi) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = v;
    }
    memcpy(s_devs, tmp, sizeof(ble_scan_result_t) * n);
    s_dev_count = n;

    // 旧行清掉重建
    for (int i = 0; i < BLE_SCAN_MAX_DEVICES; i++) s_rows[i] = NULL;
    lv_obj_clean(s_rows_box);
    for (int i = 0; i < n; i++) {
        char addr[20];
        const uint8_t *m = s_devs[i].addr;
        snprintf(addr, sizeof(addr), "%02X:%02X:%02X", m[3], m[4], m[5]);
        char label[40];
        snprintf(label, sizeof(label), "%s / %d dBm", addr, s_devs[i].rssi);

        lv_obj_t *hit = lv_obj_create(s_rows_box);
        lv_obj_remove_style_all(hit);
        lv_obj_set_pos(hit, 0, UIS(i * 108));
        lv_obj_set_size(hit, UIS(436), UIS(100));
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(hit, (void *)(intptr_t)i);
        lv_obj_add_event_cb(hit, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        const char *nm = s_devs[i].name[0] ? s_devs[i].name : "(unnamed)";
        jdm_text(hit, 16, 8, label, JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);
        jdm_text(hit, 16, 38, nm, JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_START);
        static jdm_pt_t chev[3];
        chev[0] = (jdm_pt_t){408, 40};
        chev[1] = (jdm_pt_t){418, 50};
        chev[2] = (jdm_pt_t){408, 60};
        jdm_draw_poly(hit, chev, 3, 3, JDM_ROLE_TEXT);
        static jdm_pt_t base[2];
        base[0] = (jdm_pt_t){16, 99};
        base[1] = (jdm_pt_t){420, 99};
        jdm_draw_poly(hit, base, 2, 2, JDM_ROLE_LINE);
        s_rows[i] = hit;
    }
    s_scanning = false;
    scene_apply();
}

static void scan_start(void)
{
    s_dev_count = 0;
    s_scanning = true;
    lv_obj_clean(s_rows_box);           // 清旧行
    for (int i = 0; i < BLE_SCAN_MAX_DEVICES; i++) s_rows[i] = NULL;
    elm327_ble_scan_only_start(SCAN_DURATION_S, NULL);   // 收集在客户端内部
    if (s_timer) lv_timer_del(s_timer);
    s_timer = lv_timer_create(scan_settle_cb, SCAN_DURATION_S * 1000 + SCAN_SETTLE_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
    scene_apply();
}

// ---- 绑定(NVS 真写) ----

static void adapter_bind_nv(const uint8_t mac[6], const char *name)
{
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memcpy(cfg->ble_obd_mac, mac, 6);
    snprintf(cfg->ble_device_name, sizeof(cfg->ble_device_name), "%s", name);
    nvs_cfg_set(cfg);
}

static void adapter_forget_nv(void)
{
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memset(cfg->ble_obd_mac, 0, sizeof(cfg->ble_obd_mac));
    memset(cfg->ble_device_name, 0, sizeof(cfg->ble_device_name));
    nvs_cfg_set(cfg);
}

// ---- 场景切换 ----

static void go_list(void)
{
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    s_scene = SC_LIST;
    scan_start();
    scene_apply();
}

static void go_progress(int step, const ble_scan_result_t *dev)
{
    elm327_ble_scan_only_stop();
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    if (elm327_ble_is_connected()) elm327_ble_disconnect();   // 换绑先断旧
    if (dev) {
        snprintf(s_sel_name, sizeof(s_sel_name), "%s",
                 dev->name[0] ? dev->name : "(unnamed)");
        memcpy(s_sel_mac, dev->addr, 6);
    }
    elm327_ble_connect_by_addr(s_sel_mac, s_sel_name);
    s_scene = SC_PROGRESS;
    s_step = step;
    s_deadline = lv_tick_get() + CONNECT_TIMEOUT_MS;
    scene_apply();
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    elm327_ble_scan_only_stop();
    if (s_timer) { lv_timer_del(s_timer); s_timer = NULL; }
    jdm_nav_goto(PAGE_BLUETOOTH);
}

static void rescan_cb(lv_event_t *e)
{
    (void)e;
    s_scene = SC_LIST;
    scan_start();
    scene_apply();
}

static void retry_cb(lv_event_t *e)
{
    (void)e;
    go_progress(1, NULL);
}

static void row_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= s_dev_count || s_scanning) return;
    snprintf(s_sel_name, sizeof(s_sel_name), "%s",
             s_devs[i].name[0] ? s_devs[i].name : "(unnamed)");
    memcpy(s_sel_mac, s_devs[i].addr, 6);
    go_progress(1, &s_devs[i]);
}

static void change_cb(lv_event_t *e)
{
    (void)e;
    s_scene = SC_LIST;
    scan_start();
    scene_apply();
}

// ---- 忘记对话框(真实清 NVS + 断开) ----

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
    elm327_ble_disconnect();
    adapter_forget_nv();
    dialog_close();
    s_scene = SC_LIST;
    scan_start();
    scene_apply();
}

static void forget_cb(lv_event_t *e)
{
    (void)e;
    if (s_dialog) return;
    s_dialog = true;

    s_dialog_obj = lv_obj_create(lv_obj_get_parent(s_result));
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

    jdm_text(s_dialog_obj, 360, 240, "FORGET OBD ADAPTER?", JDM_FONT_BODY, JDM_ROLE_RED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog_obj, 360, 299, s_sel_name[0] ? s_sel_name : "OBD adapter",
             JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog_obj, 360, 350, "Phone pairing is kept.", JDM_FONT_SMALL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    lv_obj_t *cancel = jdm_btn_create(s_dialog_obj, 148, 434, 192, "CANCEL", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(cancel, dialog_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *forget = jdm_btn_create(s_dialog_obj, 380, 434, 192, "FORGET", JDM_BTN_DANGER);
    lv_obj_add_event_cb(forget, dialog_forget_cb, LV_EVENT_CLICKED, NULL);
}

// ---- 构建 ----

static void build_list(lv_obj_t *page)
{
    s_list = lv_obj_create(page);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, UIS(720), UIS(720));
    lv_obj_set_pos(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    s_scan_hint = jdm_text(s_list, 142, 152, "", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);

    // 可滚动行容器:越界行上下滑动查看(LVGL 触摸滚动)
    s_rows_box = lv_obj_create(s_list);
    lv_obj_remove_style_all(s_rows_box);
    lv_obj_set_pos(s_rows_box, UIS(142), UIS(172));
    lv_obj_set_size(s_rows_box, UIS(436), UIS(332));
    lv_obj_set_scroll_dir(s_rows_box, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_rows_box, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *back = jdm_btn_create(s_list, 170, 520, 170, "BACK", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rescan = jdm_btn_create(s_list, 360, 520, 190, "RESCAN", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(rescan, rescan_cb, LV_EVENT_CLICKED, NULL);
}

static void build_progress(lv_obj_t *page)
{
    s_progress = lv_obj_create(page);
    lv_obj_remove_style_all(s_progress);
    lv_obj_set_size(s_progress, UIS(720), UIS(720));
    lv_obj_set_pos(s_progress, 0, 0);
    lv_obj_clear_flag(s_progress, LV_OBJ_FLAG_SCROLLABLE);

    // 蓝牙图标(原型同款,原点 (332,206))
    static const jdm_pt_t icon_a[] = {
        {348, 206}, {348, 285}, {380, 262}, {332, 222},
    };
    static const jdm_pt_t icon_b[] = {
        {332, 269}, {380, 229}, {348, 206},
    };
    jdm_draw_poly(s_progress, icon_a, 4, 3, JDM_ROLE_BLUE);
    jdm_draw_poly(s_progress, icon_b, 3, 3, JDM_ROLE_BLUE);

    s_step_lbl = jdm_text(s_progress, 360, 326, "CONNECTING", JDM_FONT_BODY, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    s_step_sub = jdm_text(s_progress, 360, 377, "", JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_step_hint = jdm_text(s_progress, 360, 424, "Establishing adapter connection",
                           JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    for (int i = 0; i < 3; i++) {
        lv_obj_t *dot = lv_obj_create(s_progress);
        lv_obj_remove_style_all(dot);
        lv_obj_set_pos(dot, UIS(282 + i * 56), UIS(484));
        lv_obj_set_size(dot, UIS(44), UIS(4));
        lv_obj_set_style_radius(dot, UIS(2), 0);
        lv_obj_set_style_bg_color(dot, jdm_color(JDM_ROLE_LINE), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        s_dots[i] = dot;
    }
    s_prog_btn = jdm_btn_create(s_progress, 248, 552, 224, "CANCEL", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(s_prog_btn, back_cb, LV_EVENT_CLICKED, NULL);
}

static void build_result(lv_obj_t *page)
{
    s_result = lv_obj_create(page);
    lv_obj_remove_style_all(s_result);
    lv_obj_set_size(s_result, UIS(720), UIS(720));
    lv_obj_set_pos(s_result, 0, 0);
    lv_obj_clear_flag(s_result, LV_OBJ_FLAG_SCROLLABLE);

    s_chip = jdm_chip_create(s_result, 210, 206, 300, "DATA READY", JDM_ROLE_PANEL);
    s_res_dev = jdm_text(s_result, 360, 288, "", JDM_FONT_BODY, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_res_line1 = jdm_text(s_result, 360, 350, "", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    s_res_line2 = jdm_text(s_result, 360, 386, "", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    s_res_change = jdm_btn_create(s_result, 152, 446, 200, "CHANGE", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(s_res_change, change_cb, LV_EVENT_CLICKED, NULL);
    s_res_forget = jdm_btn_create(s_result, 368, 446, 200, "FORGET", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(s_res_forget, forget_cb, LV_EVENT_CLICKED, NULL);
    s_res_retry = jdm_btn_create(s_result, 190, 446, 340, "RETRY", JDM_BTN_PRIMARY);
    lv_obj_add_event_cb(s_res_retry, retry_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = jdm_btn_create_ex(s_result, 248, 552, 224, "BACK", JDM_BTN_GHOST, true);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
}

// ---- 场景应用 ----

static void scene_apply(void)
{
    lv_obj_set_hidden(s_list, s_scene != SC_LIST);
    lv_obj_set_hidden(s_progress, s_scene != SC_PROGRESS);
    lv_obj_set_hidden(s_result, s_scene != SC_CONNECTED && s_scene != SC_DISCONNECTED);

    if (s_scene == SC_LIST) {
        lv_label_set_text(s_scan_hint,
                          s_scanning ? "SEARCHING\xE2\x80\xA6" : "");
    } else if (s_scene == SC_PROGRESS) {
        static const char *const STEPS[] = {"CONNECTING", "READING VEHICLE"};
        lv_label_set_text(s_step_lbl, STEPS[s_step - 1]);
        lv_label_set_text(s_step_sub, s_sel_name);
        lv_label_set_text(s_step_hint, s_step == 1 ? "Establishing adapter connection"
                                                   : "Adapter connected / checking data");
        for (int i = 0; i < 3; i++) {
            lv_obj_set_style_bg_color(s_dots[i],
                                      jdm_color(i <= s_step ? JDM_ROLE_AMBER : JDM_ROLE_LINE), 0);
        }
    } else {
        bool ready = s_scene == SC_CONNECTED;
        jdm_chip_set(s_chip, ready ? "DATA READY" : "CONNECTION LOST",
                     ready ? JDM_ROLE_PANEL : JDM_ROLE_AMBER);
        lv_label_set_text(s_res_dev, s_sel_name);
        lv_label_set_text(s_res_line1, ready ? "Vehicle data is available" : "Saved adapter retained");
        lv_label_set_text(s_res_line2, ready ? "This adapter reconnects automatically"
                                             : "Check power and reconnect");
        lv_obj_set_hidden(s_res_change, !ready);
        lv_obj_set_hidden(s_res_forget, !ready);
        lv_obj_set_hidden(s_res_retry, ready);
    }
}

static void create(lv_obj_t *page)
{
    // 绑定态:NVS 适配器绑定
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    bool bound = (cfg->ble_obd_mac[0] | cfg->ble_obd_mac[1] | cfg->ble_obd_mac[2] |
                  cfg->ble_obd_mac[3] | cfg->ble_obd_mac[4] | cfg->ble_obd_mac[5]) != 0;
    snprintf(s_sel_name, sizeof(s_sel_name), "%s",
             cfg->ble_device_name[0] ? cfg->ble_device_name : "OBD adapter");
    memcpy(s_sel_mac, cfg->ble_obd_mac, 6);

    jdm_header_create(page, "OBD SETUP", "SELECT AN ADAPTER");

    build_list(page);
    build_progress(page);
    build_result(page);

    s_scene = bound ? (jdm_conn_active() ? SC_CONNECTED : SC_DISCONNECTED) : SC_LIST;
    scene_apply();
    if (s_scene == SC_LIST) scan_start();   // 未绑定进入即扫
}

static void update(void)
{
    if (s_scene == SC_PROGRESS) {
        bool link = elm327_ble_is_connected();
        if (s_step == 1 && link) {          // BLE 链路建立 → 读车验证
            s_step = 2;
            s_deadline = lv_tick_get() + CONNECT_TIMEOUT_MS;
        } else if (s_step == 2 && jdm_conn_active()) {
            adapter_bind_nv(s_sel_mac, s_sel_name);   // 数据就绪 → 真实绑定
            s_scene = SC_CONNECTED;
        } else if (lv_tick_get() > s_deadline) {
            s_scene = SC_DISCONNECTED;                // 超时
        }
    }
    scene_apply();
}

const jdm_page_t page_obd = {"obd", create, update, 200};
