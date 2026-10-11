// ================================================================
//  jdm_ui.c — JDM UI 入口实现
//
//  boot 序列:splash(face 底 + "SKY GAUGE")停 ~1.2s(顺带给字体
//  预热让路)→ 建 16 页轮播。M1 阶段挂"组件画布"页(验收组件库用),
//  M2 起逐页替换为 pages/ 真页。
// ================================================================

#include "jdm_ui.h"

#include "esp_log.h"
#include "lvgl.h"

#include "core/jdm_fonts.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_alert.h"
#include "widgets/jdm_arc.h"
#include "widgets/jdm_btn.h"
#include "widgets/jdm_chip.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_needle.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_round_btn.h"
#include "widgets/jdm_shell.h"
#include "widgets/jdm_status_text.h"
#include "widgets/jdm_tach.h"
#include "widgets/jdm_thermal.h"
#include "widgets/jdm_toggle.h"
#include "widgets/jdm_trend.h"
#include "bsp_obd_dsp/nvs_storage.h"

static const char *TAG = "jdm_ui";

// ----------------------------------------------------------------
//  splash(非原型页,仅开机 1.2s;M2 视需要并入开机动画)
// ----------------------------------------------------------------

static void splash_build(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, jdm_color(JDM_ROLE_FACE), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, jdm_font(JDM_FONT_TITLE), 0);
    lv_obj_set_style_text_color(title, jdm_color(JDM_ROLE_MUTED), 0);
    lv_label_set_text(title, "SKY GAUGE");
    lv_obj_center(title);
    lv_screen_load(scr);
}

// ----------------------------------------------------------------
//  页 1:仪表组件画布(shell/header/readout/tach/arc/needle/
//  thermal/trend/status/alert —— M1 验收画布,M2 逐页替换)
// ----------------------------------------------------------------

static lv_obj_t *s_tach;
static lv_obj_t *s_arc;
static lv_obj_t *s_needle;
static lv_obj_t *s_thermal;
static jdm_trend_t s_trend;
static jdm_readout_t s_ro_primary;
static jdm_readout_t s_ro_compact;
static jdm_alert_widgets_t s_alert;

static void gallery_create(lv_obj_t *parent)
{
    jdm_shell_build(parent);
    jdm_header_create(parent, "WIDGETS", "COMPONENT CANVAS");

    // 中上:56 段转速弧(r282,顶部留给 header)
    s_tach = jdm_tach_create(parent);
    jdm_tach_update(s_tach, 4800, true);

    // 弧心:primary readout(RPM)
    jdm_readout_create(parent, 360, 300, JDM_RO_PRIMARY, &s_ro_primary);
    jdm_readout_set_label(&s_ro_primary, "RPM");
    jdm_readout_set_unit(&s_ro_primary, "rpm");
    jdm_readout_set_channel(&s_ro_primary, DISP_ITEM_RPM, 4800, true);

    // 右上:compact readout(BAT,cx=645)
    jdm_readout_create(parent, 620, 205, JDM_RO_COMPACT, &s_ro_compact);
    jdm_readout_set_label(&s_ro_compact, "BAT");
    jdm_readout_set_unit(&s_ro_compact, "V");
    jdm_readout_set_channel(&s_ro_compact, DISP_ITEM_BAT, 14200, true);

    // 左下:270° 弧 + 指针 + 面板轨道(boost 演示,圆心 190,435)
    lv_obj_t *track = lv_arc_create(parent);
    int32_t tsize = UIS(2 * 130 + 30);
    lv_obj_set_size(track, tsize, tsize);
    lv_obj_set_pos(track, UIS(190) - tsize / 2, UIS(435) - tsize / 2);
    lv_obj_remove_style(track, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(track, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(track, jdm_color(JDM_ROLE_PANEL), LV_PART_MAIN);
    lv_obj_set_style_arc_width(track, UIS(24), LV_PART_MAIN);
    lv_arc_set_rotation(track, 0);
    lv_arc_set_bg_angles(track, 135, 405);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    s_arc = jdm_arc_create(parent, 230, 445, 115, 10);
    jdm_arc_configure(s_arc, 135, 270, 400);   // −1..3 bar → 0..400
    jdm_arc_set(s_arc, 168);                   // 0.68 bar
    s_needle = jdm_needle_create_group(parent, 230, 445, 90, JDM_ROLE_TEXT);
    jdm_needle_set_angle(s_needle, 135 + 168 * 260 / 400);

    // 右下:温度柱(x142..186)+ trend(x400..590)
    s_thermal = jdm_thermal_create(parent);
    jdm_thermal_update(s_thermal, 92, true, false);

    static const int32_t samples[] = {4, 6, 5, 9, 7, 10, 6, 5, 8, 9, 6, 4};
    jdm_trend_create(parent, 400, 500, 190, 55, 16, &s_trend);
    jdm_trend_set(&s_trend, samples, 12);

    // 状态字 + 页脚报警摘要
    jdm_status_text_create(parent, 500, 585, "STABLE");
    jdm_alert_create(parent, &s_alert);
    const jdm_alert_item_t alerts[] = {
        {"BOOST HIGH", JDM_ALERT_WARNING},
        {"OIL TEMP", JDM_ALERT_ADVISORY},
    };
    jdm_alert_set(&s_alert, alerts, 2);
}

static void gallery_update(void)
{
    // 画布页静态;数据驱动路径由 M2 真页示范
}

// ----------------------------------------------------------------
//  页 2:控件画布(chip/btn/round/toggle)
// ----------------------------------------------------------------

static lv_obj_t *s_toggle;

static void controls_create(lv_obj_t *parent)
{
    jdm_shell_build(parent);
    jdm_header_create(parent, "CONTROLS", "INTERACTION CANVAS");

    jdm_chip_create(parent, 175, 176, 370, "SIM-ELM327  CONNECTED", JDM_ROLE_PANEL);
    jdm_btn_create(parent, 160, 260, 190, "PAIR", JDM_BTN_PRIMARY);
    jdm_btn_create(parent, 370, 260, 190, "FORGET", JDM_BTN_DANGER);
    jdm_btn_create(parent, 160, 370, 190, "BACK", JDM_BTN_GHOST);
    jdm_btn_create(parent, 370, 370, 190, "OPTION", JDM_BTN_SECONDARY);
    jdm_round_btn_create(parent, 180, 480, 96, "-");
    jdm_round_btn_create(parent, 440, 480, 96, "+");

    s_toggle = jdm_toggle_create(parent, 318, 560, 104, true);
}

static void controls_update(void)
{
}

static const jdm_page_t s_pages[] = {
    {"gallery", gallery_create, gallery_update, 200},
    {"controls", controls_create, controls_update, 200},
};

// ----------------------------------------------------------------

static void boot_done_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    jdm_nav_init();
    jdm_nav_set_pages(s_pages, (uint8_t)(sizeof(s_pages) / sizeof(s_pages[0])));
    ESP_LOGI(TAG, "nav up (%u pages)", (unsigned)(sizeof(s_pages) / sizeof(s_pages[0])));
}

void jdm_ui_init(int render_res)
{
    // 恢复 NVS 记忆的日/夜档与背光(board_init 的默认亮度在此被覆盖)
    jdm_theme_set_night(nvs_cfg_get()->night_mode != 0);

    jdm_fonts_init(render_res);
    splash_build();

    // splash 停留期间预热 hero 数字字形(见 jdm_fonts_prewarm)
    jdm_fonts_prewarm();

    lv_timer_t *t = lv_timer_create(boot_done_cb, 1200, NULL);
    lv_timer_set_repeat_count(t, 1);
}
