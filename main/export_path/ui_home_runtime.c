// ================================================================
//  ui_home_runtime.c — 用户自定义仪表页运行时(M4.b)
//
//  渲染:单屏重建(切换平铺时清掉内容重画,后续增量换滑动动画);
//  槽位行模型 1..6:[1]/[1,1]/[2,1]/[2,2]/[2,2,1]/[2,2,2](ref 验证过的
//  圆屏安全排布);GEAR 页 = 大字挡位 + RPM 弧;GFORCE 页 = G 力点图。
// ================================================================

#include "ui_home_runtime.h"
#include "ui_dashboard_config.h"
#include "ui.h"
#include "ui_component.h"
#include "ui_theme.h"
#include "ui_helpers.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/ui_dashboard_logic.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"

#include <stdio.h>
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

/* 前向声明(ADD/编辑态回调与平铺构建互引用) */
static void home_build_tile(uint8_t tile);
static void home_add_click_cb(lv_event_t *e);

/* 圆屏内容安全区:360 直径下,内容矩形收缩到 320×320 */
#define HOME_CONTENT_INSET   20
#define HOME_TILE_PAD        6

static lv_obj_t *s_home;             // 首页根屏
static lv_obj_t *s_content;          // 平铺内容容器(切换时重建)
static uint8_t   s_active_tile;
static bool       s_edit_mode;       // 长按编辑态(锁翻页,overlay 三区)
static uint8_t    s_edit_page;       // 编辑目标页(0 基)
static lv_obj_t *s_comp_objs[UI_DASHBOARD_MAX_SLOTS];
static uint8_t   s_comp_count;

/** 槽位行模型:返回每行槽数(总数=slot_count)。 */
static void home_row_model(uint8_t slot_count, uint8_t rows[3], uint8_t *row_count)
{
    switch (slot_count) {
    case 1: rows[0] = 1; *row_count = 1; break;
    case 2: rows[0] = 1; rows[1] = 1; *row_count = 2; break;
    case 3: rows[0] = 2; rows[1] = 1; *row_count = 2; break;
    case 4: rows[0] = 2; rows[1] = 2; *row_count = 2; break;
    case 5: rows[0] = 2; rows[1] = 2; rows[2] = 1; *row_count = 3; break;
    default: rows[0] = 2; rows[1] = 2; rows[2] = 2; *row_count = 3; break;
    }
}

static void home_clear_content(void)
{
    s_comp_count = 0;
    memset(s_comp_objs, 0, sizeof(s_comp_objs));
    if (s_content) {
        lv_obj_set_user_data(s_content, NULL);   // GEAR 页标签链,防悬垂
        lv_obj_clean(s_content);
    }
}

/* ---- MENU 平铺:车型名 + BLE/设置/版本页入口 ---- */
static void home_menu_btn_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    int which = (int)(uintptr_t)lv_obj_get_user_data(btn);
    switch (which) {
    case 0:
        _ui_screen_change(&ui_ScreenPageBLEScan, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                          &ui_ScreenPageBLEScan_screen_init);
        break;
    case 1:
        _ui_screen_change(&ui_ScreenPageSettings, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                          &ui_ScreenPageSettings_screen_init);
        break;
    default:
        _ui_screen_change(&ui_ScreenPageEasterEgg, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                          &ui_ScreenPageEasterEgg_screen_init);
        break;
    }
}

static void home_build_menu(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "MENU");
    lv_obj_set_style_text_font(title, &ui_font_FontTypoderSize24, 0);
    lv_obj_set_style_text_color(title, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *veh = lv_label_create(parent);
    lv_label_set_text(veh, vehicle_profile_get_active()->name);
    lv_obj_set_style_text_font(veh, &ui_font_FontTypoderSize16, 0);
    lv_obj_set_style_text_color(veh, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_align(veh, LV_ALIGN_TOP_MID, 0, 34);

    static const char *const entries[] = {"BLE SCAN", "SETTINGS", "INFO / OTA"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 170, 40);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 64 + i * 50);
        lv_obj_set_style_bg_color(btn, ui_theme_color_lv(UI_COLOR_PANEL), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
        lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn, home_menu_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, entries[i]);
        lv_obj_set_style_text_font(lbl, &ui_font_FontTypoderSize16, 0);
        lv_obj_set_style_text_color(lbl, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
        lv_obj_center(lbl);
    }
}

/* ---- ADD 平铺:几何 "+"(两根 bar,不用字体字形) ---- */
static void home_build_add(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "ADD PAGE");
    lv_obj_set_style_text_font(title, &ui_font_FontTypoderSize16, 0);
    lv_obj_set_style_text_color(title, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *plus = lv_obj_create(parent);
    lv_obj_set_size(plus, 110, 110);
    lv_obj_align(plus, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_radius(plus, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(plus, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), 0);
    lv_obj_set_style_border_width(plus, 2, 0);
    lv_obj_set_style_bg_opa(plus, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(plus, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *plus_h = lv_obj_create(plus);
    lv_obj_set_size(plus_h, 64, 8);
    lv_obj_align(plus_h, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(plus_h, 4, 0);
    lv_obj_set_style_bg_color(plus_h, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), 0);
    lv_obj_set_style_bg_opa(plus_h, 255, 0);
    lv_obj_set_style_border_width(plus_h, 0, 0);
    lv_obj_clear_flag(plus_h, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *plus_v = lv_obj_create(plus);
    lv_obj_set_size(plus_v, 8, 64);
    lv_obj_align(plus_v, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(plus_v, 4, 0);
    lv_obj_set_style_bg_color(plus_v, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), 0);
    lv_obj_set_style_bg_opa(plus_v, 255, 0);
    lv_obj_set_style_border_width(plus_v, 0, 0);
    lv_obj_clear_flag(plus_v, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(plus, home_add_click_cb, LV_EVENT_CLICKED, NULL);
}

/** ADD:追加一个默认页(1 槽 RPM),跳到新页;满 8 页提示。 */
static void home_add_click_cb(lv_event_t *e)
{
    (void)e;
    ui_dashboard_cfg_t *dash = &nvs_cfg_get()->dashboard;   // 修改走 mutator 持久化
    if (dash->page_count >= UI_DASHBOARD_MAX_PAGES) {
        ESP_LOGW("home", "dashboard full (%d pages)", dash->page_count);
        return;
    }
    ui_dashboard_page_cfg_t np;
    memset(&np, 0, sizeof(np));
    np.type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    np.slot_count = 1u;
    np.slot_items[0] = (uint8_t)DISP_ITEM_RPM;
    if (nvs_dashboard_page_append(&np) != ESP_OK) return;
    s_active_tile = (uint8_t)(nvs_cfg_get()->dashboard.page_count);  // 新页平铺
    home_build_tile(s_active_tile);
}

/* ---- GEAR 平铺:大字挡位 + 小 RPM 弧 ---- */
static void home_build_gear(lv_obj_t *parent)
{
    ui_comp_desc_t d = { .type = UI_COMP_BIG_NUM, .channel = DISP_ITEM_RPM,
                         .x = 0, .y = 0, .w = 200, .h = 120 };
    // RPM 以弧呈现,挡位大字放中央(挡位非通道,直接静态标签 + 刷新钩子)
    lv_obj_t *gear = lv_label_create(parent);
    lv_label_set_text(gear, "-");
    lv_obj_set_style_text_font(gear, &ui_font_FontTypoderSize140, 0);
    lv_obj_set_style_text_color(gear, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(gear, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_user_data(parent, gear);   // GEAR 页:user_data 挂挡位标签

    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 150, 150);
    lv_obj_align(arc, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, 8000);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc, ui_theme_color_lv(UI_COLOR_ARC_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, ui_theme_color_lv(UI_COLOR_ARC_INDICATOR), LV_PART_INDICATOR);
    lv_obj_set_user_data(gear, arc);      // 挡位标签 user_data 挂 RPM 弧
    (void)d;
}

/* ---- GFORCE 平铺 ---- */
static void home_build_gforce(lv_obj_t *parent)
{
    ui_comp_desc_t d = { .type = UI_COMP_GFORCE, .channel = DISP_ITEM_GFORCE_LAT,
                         .x = 65, .y = 65, .w = 230, .h = 230 };
    lv_obj_t *c = ui_comp_create(&d, parent);
    (void)c;
}

/* ---- METRIC 平铺:槽位网格 ---- */
static void home_build_metric(lv_obj_t *parent, const ui_dashboard_page_cfg_t *page)
{
    const int area = 360 - 2 * HOME_CONTENT_INSET;
    uint8_t rows[3], row_count;
    home_row_model(page->slot_count, rows, &row_count);

    int y = HOME_CONTENT_INSET;
    uint8_t slot = 0;
    for (uint8_t r = 0; r < row_count; r++) {
        int row_h = (area - (row_count - 1) * HOME_TILE_PAD) / row_count;
        int x = HOME_CONTENT_INSET;
        for (uint8_t c = 0; c < rows[r]; c++) {
            int cell_w = (area - (rows[r] - 1) * HOME_TILE_PAD) / rows[r];
            ui_comp_desc_t d = {
                .type = UI_COMP_VALUE,
                .channel = (disp_item_t)page->slot_items[slot],
                .x = (int16_t)x, .y = (int16_t)y,
                .w = (int16_t)cell_w, .h = (int16_t)row_h,
            };
            lv_obj_t *comp = ui_comp_create(&d, parent);
            if (comp && s_comp_count < UI_DASHBOARD_MAX_SLOTS) {
                s_comp_objs[s_comp_count++] = comp;
            }
            slot++;
            x += cell_w + HOME_TILE_PAD;
        }
        y += row_h + HOME_TILE_PAD;
    }
}

static void home_build_tile(uint8_t tile)
{
    home_clear_content();
    const ui_dashboard_cfg_t *dash = &nvs_cfg_get()->dashboard;
    uint8_t page = ui_dashboard_logic_tile_to_page(tile, dash->page_count);

    if (tile == UI_HOME_PAGE_MENU) {
        home_build_menu(s_content);
        return;
    }
    if (page >= dash->page_count) {   // ADD 平铺
        home_build_add(s_content);
        return;
    }
    const ui_dashboard_page_cfg_t *p = &dash->pages[page];
    switch ((ui_dashboard_page_type_t)p->type) {
    case UI_DASHBOARD_PAGE_GEAR:   home_build_gear(s_content);   break;
    case UI_DASHBOARD_PAGE_GFORCE: home_build_gforce(s_content); break;
    default:                       home_build_metric(s_content, p); break;
    }
}

static void home_gesture_cb(lv_event_t *e)
{
    if (s_edit_mode) return;   // 编辑态锁翻页(overlay 按钮退出)
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  { (void)ui_home_step(+1); return; }
    if (dir == LV_DIR_RIGHT) { (void)ui_home_step(-1); return; }
    // 对齐 ref 约定:MENU 平铺上滑 → BLE 扫描、下滑 → 设置(仅 MENU;
    // 仪表页的上下滑预留给报警阈值配置)。按钮入口并存,防手势误触。
    if (s_active_tile != UI_HOME_PAGE_MENU) return;
    if (dir == LV_DIR_TOP) {
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_ScreenPageBLEScan, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                          &ui_ScreenPageBLEScan_screen_init);
    } else if (dir == LV_DIR_BOTTOM) {
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(&ui_ScreenPageSettings, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0,
                          &ui_ScreenPageSettings_screen_init);
    }
}

/* ---- 编辑态(M4.d):长按仪表页 → 三区 overlay ---- */

static void home_edit_exit(void)
{
    s_edit_mode = false;
    lv_obj_t *ov = (lv_obj_t *)lv_obj_get_user_data(s_content);
    if (ov) lv_obj_del(ov);
    lv_obj_set_user_data(s_content, NULL);
    home_build_tile(s_active_tile);   // 删除/重配置后按 NVS 现状重建
}

static void home_edit_back_cb(lv_event_t *e)
{
    (void)e;
    home_edit_exit();
}

static void home_edit_delete_cb(lv_event_t *e)
{
    (void)e;
    uint8_t page = ui_dashboard_logic_tile_to_page(s_active_tile,
                                                    nvs_cfg_get()->dashboard.page_count);
    if (nvs_cfg_get()->dashboard.page_count <= 1) return;
    if (nvs_dashboard_page_delete(page) != ESP_OK) return;
    // 删除后收敛到 MENU
    s_active_tile = UI_HOME_PAGE_MENU;
    home_edit_exit();
    s_active_tile = UI_HOME_PAGE_MENU;
    home_build_tile(s_active_tile);
}

static void home_edit_reconfig_cb(lv_event_t *e)
{
    (void)e;
    uint8_t page = ui_dashboard_logic_tile_to_page(s_active_tile,
                                                    nvs_cfg_get()->dashboard.page_count);
    s_edit_mode = false;
    lv_obj_t *ov = (lv_obj_t *)lv_obj_get_user_data(s_content);
    if (ov) lv_obj_del(ov);
    lv_obj_set_user_data(s_content, NULL);
    ui_dashboard_config_open(page);   // 返回时由 ui_event_home_return 重建
}

static void home_edit_overlay_build(void)
{
    lv_obj_t *ov = lv_obj_create(s_home);
    lv_obj_set_size(ov, 360, 360);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(s_content, ov);

    struct { const char *txt; lv_color_t bg; lv_align_t align; int xo, yo;
             lv_event_cb_t cb; } zones[] = {
        { "EDIT",   lv_color_hex(0x2266CC), LV_ALIGN_TOP_LEFT,     0, 0, home_edit_reconfig_cb },
        { "DELETE", lv_color_hex(0xCC3333), LV_ALIGN_TOP_RIGHT,    0, 0, home_edit_delete_cb },
        { "BACK",   lv_color_hex(0x22AA55), LV_ALIGN_BOTTOM_MID,   0, 0, home_edit_back_cb },
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(ov);
        if (i == 2) lv_obj_set_size(btn, 360, 70);
        else        lv_obj_set_size(btn, 178, 70);
        lv_obj_align(btn, zones[i].align, zones[i].xo, zones[i].yo);
        lv_obj_set_style_bg_color(btn, zones[i].bg, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(btn, zones[i].cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, zones[i].txt);
        lv_obj_set_style_text_font(lbl, &ui_font_FontTypoderSize20, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
    }
}

static void home_longpress_cb(lv_event_t *e)
{
    (void)e;
    if (s_edit_mode) return;
    const ui_dashboard_cfg_t *dash = &nvs_cfg_get()->dashboard;
    uint8_t page = ui_dashboard_logic_tile_to_page(s_active_tile, dash->page_count);
    if (s_active_tile == UI_HOME_PAGE_MENU || page >= dash->page_count) return;  // MENU/ADD 不可编辑
    s_edit_page = page;
    s_edit_mode = true;
    home_edit_overlay_build();
}

static void home_refresh_timer_cb(lv_timer_t *t)
{
    (void)t;
    ui_home_refresh();
}

lv_obj_t *ui_home_init(void)
{
    if (s_home) return s_home;

    s_home = lv_obj_create(NULL);
    lv_obj_set_size(s_home, 360, 360);
    lv_obj_clear_flag(s_home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_home, ui_theme_color_lv(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_home, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_home, 0, 0);
    lv_obj_t *ring = ui_helpers_create_ring(s_home, 10);

    s_content = lv_obj_create(s_home);
    lv_obj_set_size(s_content, 360, 360);
    lv_obj_set_pos(s_content, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);

    const ui_dashboard_cfg_t *dash = &nvs_cfg_get()->dashboard;
    s_active_tile = (dash->default_page >= 1u && dash->default_page <= dash->page_count)
                        ? (uint8_t)(dash->default_page) : UI_HOME_PAGE_MENU;
    home_build_tile(s_active_tile);

    lv_obj_add_event_cb(s_home, home_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_content, home_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_timer_create(home_refresh_timer_cb, 100, NULL);   // 数据刷新(固件/模拟器共用)

    lv_obj_move_foreground(ring);
    return s_home;
}

/** 配置页等子页面的"返回 home"手势处理(ui_dashboard_config 引用)。 */
void ui_event_home_return(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_GESTURE) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    lv_indev_wait_release(lv_indev_get_act());
    lv_scr_load_anim(ui_home_get(), LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
}

lv_obj_t *ui_home_get(void)
{
    return s_home ? s_home : ui_home_init();
}

uint8_t ui_home_active_tile(void)
{
    return s_active_tile;
}

bool ui_home_step(int dir)
{
    const ui_dashboard_cfg_t *dash = &nvs_cfg_get()->dashboard;
    uint8_t tiles = ui_dashboard_logic_tile_count(dash->page_count);
    int next = (int)s_active_tile + dir;
    if (next < 0 || next >= (int)tiles) return false;
    s_active_tile = (uint8_t)next;
    home_build_tile(s_active_tile);
    return true;
}

void ui_home_refresh(void)
{
    // METRIC 平铺:组件自读缓存
    for (uint8_t i = 0; i < s_comp_count; i++) {
        if (s_comp_objs[i]) ui_comp_update(s_comp_objs[i]);
    }
    // GEAR 平铺:挡位标签 + RPM 弧(挂在 s_content->user_data 链上)
    lv_obj_t *gear = (lv_obj_t *)lv_obj_get_user_data(s_content);
    if (gear && lv_obj_check_type(gear, &lv_label_class)) {
        static int8_t last_gear = 127;
        int8_t g = obd_data_get_gear();
        if (g != last_gear) {
            char buf[2] = { '-', 0 };
            if (g == -1) buf[0] = 'R';
            else if (g == 0) buf[0] = 'N';
            else if (g >= 1 && g <= 8) buf[0] = (char)('0' + g);
            last_gear = g;
            lv_label_set_text(gear, buf);
        }
        lv_obj_t *arc = (lv_obj_t *)lv_obj_get_user_data(gear);
        if (arc && lv_obj_check_type(arc, &lv_arc_class)) {
            lv_arc_set_value(arc, (int16_t)obd_data_get_rpm());
        }
    }
}
