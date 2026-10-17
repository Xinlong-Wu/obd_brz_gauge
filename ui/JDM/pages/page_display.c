// ================================================================
//  page_display.c — 日间 / 夜间亮度(原型 14 页 ILLUMINATION)
//
//  DAY/NIGHT 两档各:标签 + 百分比 + 进度槽 + ± 圆键(±5,范围
//  10..100,越界禁用);底部模式切换按钮。调档经 jdm_theme 即时生效。
// ================================================================

#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_round_btn.h"
#include "bsp_obd_dsp/nvs_storage.h"

#include <stdio.h>

static lv_obj_t *s_day_pct;
static lv_obj_t *s_day_bar;
static lv_obj_t *s_night_pct;
static lv_obj_t *s_night_bar;
static lv_obj_t *s_day_minus;
static lv_obj_t *s_day_plus;
static lv_obj_t *s_night_minus;
static lv_obj_t *s_night_plus;
static lv_obj_t *s_mode_btn;

static lv_obj_t *bar_create(lv_obj_t *parent, int32_t my)
{
    lv_obj_t *slot = lv_obj_create(parent);
    lv_obj_remove_style_all(slot);
    lv_obj_set_pos(slot, UIS(146), UIS(my));
    lv_obj_set_size(slot, UIS(428), UIS(8));
    lv_obj_set_style_radius(slot, UIS(4), 0);
    lv_obj_set_style_bg_color(slot, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(slot, LV_OPA_COVER, 0);

    lv_obj_t *fill = lv_obj_create(slot);
    lv_obj_remove_style_all(fill);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, 1, UIS(8));
    lv_obj_set_style_radius(fill, UIS(4), 0);
    lv_obj_set_style_bg_color(fill, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    return fill;
}

static void refresh(void)
{
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    uint8_t day = cfg->brightness_day ? cfg->brightness_day : 85;
    uint8_t night = cfg->brightness_night ? cfg->brightness_night : 35;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", day);
    lv_label_set_text(s_day_pct, buf);
    lv_obj_set_width(s_day_bar, UIS(428) * day / 100);
    snprintf(buf, sizeof(buf), "%d%%", night);
    lv_label_set_text(s_night_pct, buf);
    lv_obj_set_width(s_night_bar, UIS(428) * night / 100);

    jdm_round_btn_set_disabled(s_day_minus, day <= 10);
    jdm_round_btn_set_disabled(s_day_plus, day >= 100);
    jdm_round_btn_set_disabled(s_night_minus, night <= 10);
    jdm_round_btn_set_disabled(s_night_plus, night >= 100);
    jdm_btn_set_label(s_mode_btn,
                      jdm_theme_is_night() ? "NIGHT ACTIVE" : "DAY ACTIVE");
}

static void day_minus_cb(lv_event_t *e) { (void)e; jdm_theme_adjust_brightness(false, -5); refresh(); }
static void day_plus_cb(lv_event_t *e)  { (void)e; jdm_theme_adjust_brightness(false, +5); refresh(); }
static void night_minus_cb(lv_event_t *e) { (void)e; jdm_theme_adjust_brightness(true, -5); refresh(); }
static void night_plus_cb(lv_event_t *e)  { (void)e; jdm_theme_adjust_brightness(true, +5); refresh(); }
static void mode_cb(lv_event_t *e) { (void)e; jdm_theme_set_night(!jdm_theme_is_night()); refresh(); }

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "ILLUMINATION", "DAY / NIGHT MEMORY");

    jdm_text(page, 146, 188, "DAY", JDM_FONT_TITLE, JDM_ROLE_TEXT, JDM_TEXT_START);
    s_day_pct = jdm_text(page, 574, 188, "85%", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_END);
    s_day_bar = bar_create(page, 242);
    s_day_minus = jdm_round_btn_create(page, 146, 264, 96, "\xE2\x88\x92");
    lv_obj_add_event_cb(s_day_minus, day_minus_cb, LV_EVENT_CLICKED, NULL);
    s_day_plus = jdm_round_btn_create(page, 478, 264, 96, "+");
    lv_obj_add_event_cb(s_day_plus, day_plus_cb, LV_EVENT_CLICKED, NULL);

    jdm_text(page, 146, 368, "NIGHT", JDM_FONT_TITLE, JDM_ROLE_TEXT, JDM_TEXT_START);
    s_night_pct = jdm_text(page, 574, 368, "35%", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_END);
    s_night_bar = bar_create(page, 422);
    s_night_minus = jdm_round_btn_create(page, 146, 444, 96, "\xE2\x88\x92");
    lv_obj_add_event_cb(s_night_minus, night_minus_cb, LV_EVENT_CLICKED, NULL);
    s_night_plus = jdm_round_btn_create(page, 478, 444, 96, "+");
    lv_obj_add_event_cb(s_night_plus, night_plus_cb, LV_EVENT_CLICKED, NULL);

    s_mode_btn = jdm_btn_create(page, 240, 552, 240, "DAY ACTIVE", JDM_BTN_PRIMARY);
    lv_obj_add_event_cb(s_mode_btn, mode_cb, LV_EVENT_CLICKED, NULL);

    refresh();
}

static void update(void)
{
    // 交互页;状态由回调驱动
}

const jdm_page_t page_display = {"display", create, update, 200};
