// ================================================================
//  ui_dashboard_config.c — 仪表页滚轮配置页(M4.d)
//
//  三行滚轮(圆屏 360 内纵向排布):TYPE / SLOTS / CHANNEL。
//  CHANNEL 行编辑"当前槽",当前槽由 SLOTS 行选择后固定为最后增/减的
//  槽(简化:总是编辑槽 0..slot_count-1 的"当前槽"游标,滑 SLOTS 时
//  重置)。所有变更即持久化;左右滑返回 home。
// ================================================================

#include "ui_dashboard_config.h"
#include "ui_home_runtime.h"
#include "ui_theme.h"
#include "ui_helpers.h"
#include "ui_disp_item.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/ui_dashboard_logic.h"

#include <stdio.h>
#include <string.h>
#include "ui_res.h"

static uint8_t  s_page_idx;
static lv_obj_t *s_roller_type;
static lv_obj_t *s_roller_slots;
static lv_obj_t *s_roller_slot;   // 编辑哪个槽(1..N)
static lv_obj_t *s_roller_chan;
static uint8_t   s_edit_slot;     // 0 基当前槽

/* ---- 滚轮通用样式(与设置页一致) ---- */
static lv_obj_t *make_roller(lv_obj_t *parent, const char *options,
                             int y, int width, lv_event_cb_t cb)
{
    lv_obj_t *r = lv_roller_create(parent);
    lv_obj_set_style_clip_corner(r, true, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_roller_set_options(r, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(r, 1);
    lv_obj_set_width(r, width);
    lv_obj_set_height(r, UIS(60));
    ui_helpers_style_dark_roller(r, &ui_font_FontTypoderSize20);
    lv_obj_align(r, LV_ALIGN_CENTER, 0, y);
    if (cb) lv_obj_add_event_cb(r, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return r;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *txt, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &ui_font_FontTypoderSize16, 0);
    lv_obj_set_style_text_color(l, ui_theme_color_lv(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, y);
    return l;
}

/* ---- 通道滚轮选项:18 项 name 拼接(静态缓冲,DISP_ITEM_COUNT 上限) ---- */
static void channel_options(char *buf, size_t len)
{
    buf[0] = '\0';
    for (int i = 0; i < (int)DISP_ITEM_COUNT; i++) {
        if (i > 0) strlcat(buf, "\n", len);
        strlcat(buf, ui_disp_item_name((uint8_t)i), len);
    }
}

static void sync_channel_roller(void)
{
    const ui_dashboard_page_cfg_t *p = &nvs_cfg_get()->dashboard.pages[s_page_idx];
    if (s_edit_slot >= p->slot_count) s_edit_slot = 0;
    lv_roller_set_selected(s_roller_chan, p->slot_items[s_edit_slot], LV_ANIM_OFF);
}

static void on_type_change(lv_event_t *e)
{
    (void)e;
    ui_dashboard_page_cfg_t p = nvs_cfg_get()->dashboard.pages[s_page_idx];
    p.type = (uint8_t)lv_roller_get_selected(s_roller_type);
    nvs_dashboard_page_set(s_page_idx, &p);
    // GEAR/GFORCE 无槽位概念:SLOTS/CHANNEL 滚轮隐藏
    bool metric = (p.type == (uint8_t)UI_DASHBOARD_PAGE_METRIC);
    lv_obj_add_flag(s_roller_slots, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_roller_slot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_roller_chan, LV_OBJ_FLAG_HIDDEN);
    if (metric) {
        lv_obj_clear_flag(s_roller_slots, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_roller_slot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_roller_chan, LV_OBJ_FLAG_HIDDEN);
    }
}

static void on_slots_change(lv_event_t *e)
{
    (void)e;
    ui_dashboard_page_cfg_t p = nvs_cfg_get()->dashboard.pages[s_page_idx];
    p.slot_count = (uint8_t)(lv_roller_get_selected(s_roller_slots) + 1);
    nvs_dashboard_page_set(s_page_idx, &p);
    s_edit_slot = 0;
    sync_channel_roller();
    lv_roller_set_selected(s_roller_slot, 0, LV_ANIM_OFF);
}

static void on_slot_change(lv_event_t *e)
{
    (void)e;
    s_edit_slot = (uint8_t)lv_roller_get_selected(s_roller_slot);
    sync_channel_roller();
}

static void on_channel_change(lv_event_t *e)
{
    (void)e;
    ui_dashboard_page_cfg_t p = nvs_cfg_get()->dashboard.pages[s_page_idx];
    p.slot_items[s_edit_slot] = (uint8_t)lv_roller_get_selected(s_roller_chan);
    nvs_dashboard_page_set(s_page_idx, &p);
}

void ui_dashboard_config_open(uint8_t page_idx)
{
    s_page_idx = page_idx;
    s_edit_slot = 0;
    const ui_dashboard_page_cfg_t *p = &nvs_cfg_get()->dashboard.pages[page_idx];

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, UIS(720), UIS(720));
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, UIS(720), LV_PART_MAIN);
    ui_helpers_style_screen_bg(scr);
    lv_obj_set_style_bg_opa(scr, 255, LV_PART_MAIN);

    lv_obj_t *ring = ui_helpers_create_ring(scr, 10);
    (void)ring;

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "DASHBOARD");
    lv_obj_set_style_text_font(title, &ui_font_FontTypoderSize24, 0);
    lv_obj_set_style_text_color(title, ui_theme_color_lv(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, UIS(-280));

    make_label(scr, "TYPE", -104);
    s_roller_type = make_roller(scr, "METRIC\nGEAR\nG-FORCE", -82, 150, on_type_change);
    lv_roller_set_selected(s_roller_type, p->type, LV_ANIM_OFF);

    make_label(scr, "SLOTS", -46);
    s_roller_slots = make_roller(scr, "1\n2\n3\n4\n5\n6", -24, 110, on_slots_change);
    lv_roller_set_selected(s_roller_slots, p->slot_count - 1, LV_ANIM_OFF);

    make_label(scr, "SLOT", 12);
    s_roller_slot = make_roller(scr, "1\n2\n3\n4\n5\n6", 34, 110, on_slot_change);
    lv_roller_set_selected(s_roller_slot, 0, LV_ANIM_OFF);

    make_label(scr, "CHANNEL", 70);
    char opts[220];
    channel_options(opts, sizeof(opts));
    s_roller_chan = make_roller(scr, opts, 96, 190, on_channel_change);
    sync_channel_roller();

    // 非 METRIC 页隐藏槽位行
    if (p->type != (uint8_t)UI_DASHBOARD_PAGE_METRIC) {
        lv_obj_add_flag(s_roller_slots, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_roller_slot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_roller_chan, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "L/R swipe: back");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x555555), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, UIS(296));

    // 左右滑返回 home(重配置经 nvs 持久化,home 重建自然生效)
    lv_obj_add_event_cb(scr, ui_event_home_return, LV_EVENT_GESTURE, NULL);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, true);
}
