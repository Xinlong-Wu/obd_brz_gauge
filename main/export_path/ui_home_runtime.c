// ================================================================
//  ui_home_runtime.c — 用户自定义仪表页运行时(M4.b)
//
//  渲染:单屏重建(切换平铺时清掉内容重画,后续增量换滑动动画);
//  槽位行模型 1..6:[1]/[1,1]/[2,1]/[2,2]/[2,2,1]/[2,2,2](ref 验证过的
//  圆屏安全排布);GEAR 页 = 大字挡位 + RPM 弧;GFORCE 页 = G 力点图。
// ================================================================

#include "ui_home_runtime.h"
#include "ui_component.h"
#include "ui_theme.h"
#include "ui_helpers.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/ui_dashboard_logic.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"

#include <stdio.h>
#include <string.h>

/* 圆屏内容安全区:360 直径下,内容矩形收缩到 320×320 */
#define HOME_CONTENT_INSET   20
#define HOME_TILE_PAD        6

static lv_obj_t *s_home;             // 首页根屏
static lv_obj_t *s_content;          // 平铺内容容器(切换时重建)
static uint8_t   s_active_tile;
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

/* ---- MENU 平铺:车型名 + BLE/设置入口(交互接线随下一增量) ---- */
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

    const char *const entries[] = {"BLE SCAN", "SETTINGS"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 170, 44);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 70 + i * 54);
        lv_obj_set_style_bg_color(btn, ui_theme_color_lv(UI_COLOR_PANEL), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
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
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
    if (dir == LV_DIR_LEFT)  (void)ui_home_step(+1);
    if (dir == LV_DIR_RIGHT) (void)ui_home_step(-1);
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
    lv_timer_create(home_refresh_timer_cb, 100, NULL);   // 数据刷新(固件/模拟器共用)

    lv_obj_move_foreground(ring);
    return s_home;
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
