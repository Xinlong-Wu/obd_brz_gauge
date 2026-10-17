// ================================================================
//  page_obd.c — OBD 设备(原型 17 页 OBD SETUP)
//
//  三态:列表(SELECT AN ADAPTER,3 行示例设备)/ 进度(SEARCHING →
//  CONNECTING → READING VEHICLE,定时推进)/ 结果(DATA READY 与
//  CONNECTION LOST)。绑定来源:NVS 适配器绑定(创建时读入 RAM);
//  FORGET 走真实 NVS 清除;列表点选为演示进度,不写 NVS。
//  BACK/CANCEL 回连接总览页(14)。
// ================================================================

#include "core/jdm_conn.h"
#include "core/jdm_draw.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_chip.h"
#include "widgets/jdm_header.h"

#include <stdio.h>
#include <string.h>

#define PAGE_BLUETOOTH  14      // 连接总览(见 jdm_ui.c 页表)

typedef enum {
    SC_LIST,                    // 设备列表
    SC_PROGRESS,                // 扫描/连接/读车进度(s_step 0..2)
    SC_CONNECTED,               // DATA READY
    SC_DISCONNECTED,            // CONNECTION LOST
} obd_scene_t;

// 原型示例设备(scenarios.js obdDevices)
static const struct {
    const char *addr;
    const char *signal;
    const char *name;
} DEVS[3] = {
    {"\xE2\x80\xA6 A4:7C", "NEARBY", "OBDLink CX"},
    {"\xE2\x80\xA6 2B:91", "NEARBY", "Vgate iCar Pro"},
    {"\xE2\x80\xA6 8D:03", "WEAK SIGNAL", "OBDII"},
};

static obd_scene_t s_scene;
static int s_step;                      // PROGRESS:0=SEARCHING 1=CONNECTING 2=READING
static bool s_ram_bound;                // 绑定态(创建时取 NVS;演示选中只改 RAM)
static char s_ram_device[32];           // 当前绑定设备名(同上)
static char s_candidate[32];            // 进度中的目标设备名
static lv_timer_t *s_timer;             // 进度演示推进
static bool s_dialog;
static lv_obj_t *s_dialog_obj;

static lv_obj_t *s_list;
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
static void start_progress(int step, const char *candidate);

// ---- 场景推进 ----

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    s_timer = NULL;
    if (s_scene != SC_PROGRESS) return;
    if (s_step == 0) {                  // 扫描完 → 回列表(原型 scanning→normal)
        s_scene = SC_LIST;
    } else if (s_step == 1) {
        s_step = 2;                     // connecting → checking
        start_progress(2, s_candidate);
        return;
    } else {                            // checking → connected(演示绑定,不写 NVS)
        s_ram_bound = true;
        strncpy(s_ram_device, s_candidate, sizeof(s_ram_device) - 1);
        s_ram_device[sizeof(s_ram_device) - 1] = '\0';
        s_scene = SC_CONNECTED;
    }
    scene_apply();
}

static void start_progress(int step, const char *candidate)
{
    if (candidate) {
        strncpy(s_candidate, candidate, sizeof(s_candidate) - 1);
        s_candidate[sizeof(s_candidate) - 1] = '\0';
    }
    s_scene = SC_PROGRESS;
    s_step = step;
    scene_apply();
    uint32_t ms = (step == 0) ? 1400 : 1100;    // 原型 demo 节奏
    s_timer = lv_timer_create(timer_cb, ms, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}

static void cancel_progress(void)
{
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    s_scene = SC_LIST;
    scene_apply();
}

// ---- 动作 ----

static void back_cb(lv_event_t *e)
{
    (void)e;
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    jdm_nav_goto(PAGE_BLUETOOTH);
}

static void rescan_cb(lv_event_t *e)
{
    (void)e;
    start_progress(0, NULL);
}

static void retry_cb(lv_event_t *e)
{
    (void)e;
    start_progress(1, s_ram_device);
}

static void row_cb(lv_event_t *e)
{
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    start_progress(1, DEVS[i].name);    // 原型 select-obd-N → connecting
}

static void change_cb(lv_event_t *e)
{
    (void)e;
    start_progress(0, NULL);
}

// ---- 忘记对话框(真实清除 NVS 绑定) ----

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
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    memset(cfg->ble_device_name, 0, sizeof(cfg->ble_device_name));
    memset(cfg->ble_obd_mac, 0, sizeof(cfg->ble_obd_mac));
    nvs_cfg_set(cfg);
    s_ram_bound = false;
    s_ram_device[0] = '\0';
    dialog_close();
    s_scene = SC_LIST;
    scene_apply();
}

static void forget_cb(lv_event_t *e)
{
    (void)e;
    if (s_dialog) return;       // 防重入
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
    jdm_text(s_dialog_obj, 360, 299, s_ram_device[0] ? s_ram_device : "OBDLink CX",
             JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_text(s_dialog_obj, 360, 350, "Phone pairing is kept.", JDM_FONT_SMALL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    lv_obj_t *cancel = jdm_btn_create(s_dialog_obj, 148, 434, 192, "CANCEL", JDM_BTN_SECONDARY);
    lv_obj_add_event_cb(cancel, dialog_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *forget = jdm_btn_create(s_dialog_obj, 380, 434, 192, "FORGET", JDM_BTN_DANGER);
    lv_obj_add_event_cb(forget, dialog_forget_cb, LV_EVENT_CLICKED, NULL);
}

// ---- 列表行(connectionRow) ----

static lv_obj_t *row_create(lv_obj_t *parent, int i, int32_t my)
{
    const char *label = DEVS[i].addr;
    static char labels[3][24];
    snprintf(labels[i], sizeof(labels[i]), "%s / %s", DEVS[i].addr, DEVS[i].signal);

    lv_obj_t *hit = lv_obj_create(parent);
    lv_obj_remove_style_all(hit);
    lv_obj_set_pos(hit, UIS(142), UIS(my));
    lv_obj_set_size(hit, UIS(436), UIS(100));
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(hit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(hit, (void *)(intptr_t)i);
    lv_obj_add_event_cb(hit, row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    bool selected = s_ram_bound && strcmp(s_ram_device, DEVS[i].name) == 0;
    jdm_color_role_t ink = selected ? JDM_ROLE_AMBER : JDM_ROLE_TEXT;
    // 行内坐标相对 hit 容器(原型 connectionRow:x+16 / y+8|38|72 / 底线 y+99)
    jdm_text(hit, 16, 8, labels[i], JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);
    jdm_text(hit, 16, 38, DEVS[i].name, JDM_FONT_LABEL, ink, JDM_TEXT_START);
    if (selected) {
        jdm_text(hit, 16, 72, "SAVED ADAPTER", JDM_FONT_SMALL, JDM_ROLE_GREEN, JDM_TEXT_START);
    }
    // 右侧 chevron(hit 局部:x=436−28..−18,y=40..60)
    static jdm_pt_t chev[3];
    chev[0] = (jdm_pt_t){408, 40};
    chev[1] = (jdm_pt_t){418, 50};
    chev[2] = (jdm_pt_t){408, 60};
    jdm_draw_poly(hit, chev, 3, 3, ink);
    // 底线
    static jdm_pt_t base[2];
    base[0] = (jdm_pt_t){16, 99};
    base[1] = (jdm_pt_t){420, 99};
    jdm_draw_poly(hit, base, 2, 2, selected ? JDM_ROLE_AMBER : JDM_ROLE_LINE);
    return hit;
}

static void build_list(lv_obj_t *page)
{
    s_list = lv_obj_create(page);
    lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list, UIS(720), UIS(720));
    lv_obj_set_pos(s_list, 0, 0);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 3; i++) {
        row_create(s_list, i, 178 + i * 108);
    }
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

    s_step_lbl = jdm_text(s_progress, 360, 326, "SEARCHING", JDM_FONT_BODY, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    s_step_sub = jdm_text(s_progress, 360, 377, "Keep the adapter powered", JDM_FONT_LABEL, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
    s_step_hint = jdm_text(s_progress, 360, 424, "Looking for nearby OBD adapters",
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
    s_prog_btn = jdm_btn_create(s_progress, 248, 552, 224, "BACK", JDM_BTN_SECONDARY);
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
    s_res_dev = jdm_text(s_result, 360, 288, "OBDLink CX", JDM_FONT_BODY, JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
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

    if (s_scene == SC_PROGRESS) {
        static const char *const STEPS[] = {"SEARCHING", "CONNECTING", "READING VEHICLE"};
        static const char *const SUBS[] = {"Keep the adapter powered", "Establishing adapter connection",
                                           "Adapter connected / checking data"};
        static const char *const HINTS[] = {"Looking for nearby OBD adapters", "Establishing adapter connection",
                                            "Adapter connected / checking data"};
        lv_label_set_text(s_step_lbl, STEPS[s_step]);
        lv_label_set_text(s_step_sub, s_step == 0 ? SUBS[0] : s_candidate);
        lv_label_set_text(s_step_hint, HINTS[s_step]);
        for (int i = 0; i < 3; i++) {
            lv_obj_set_style_bg_color(s_dots[i],
                                      jdm_color(i <= s_step ? JDM_ROLE_AMBER : JDM_ROLE_LINE), 0);
        }
        jdm_btn_set_label(s_prog_btn, s_step == 0 ? "BACK" : "CANCEL");
    } else if (s_scene != SC_LIST) {
        bool ready = s_scene == SC_CONNECTED;
        jdm_chip_set(s_chip, ready ? "DATA READY" : "CONNECTION LOST",
                     ready ? JDM_ROLE_PANEL : JDM_ROLE_AMBER);
        lv_label_set_text(s_res_dev, s_ram_device[0] ? s_ram_device : "OBDLink CX");
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
    // 绑定态:NVS 适配器绑定读入 RAM(FORGET 真清 NVS;演示选中不写)
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    bool bound = (cfg->ble_obd_mac[0] | cfg->ble_obd_mac[1] | cfg->ble_obd_mac[2] |
                  cfg->ble_obd_mac[3] | cfg->ble_obd_mac[4] | cfg->ble_obd_mac[5]) != 0;
    s_ram_bound = bound;
    strncpy(s_ram_device, cfg->ble_device_name[0] ? cfg->ble_device_name : "OBDLink CX",
            sizeof(s_ram_device) - 1);
    s_ram_device[sizeof(s_ram_device) - 1] = '\0';

    jdm_header_create(page, "OBD SETUP", "SELECT AN ADAPTER");

    build_list(page);
    build_progress(page);
    build_result(page);

    s_scene = bound ? (jdm_conn_active() ? SC_CONNECTED : SC_DISCONNECTED) : SC_LIST;
    scene_apply();
}

static void update(void)
{
    scene_apply();
}

const jdm_page_t page_obd = {"obd", create, update, 200};
