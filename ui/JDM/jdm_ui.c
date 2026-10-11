// ================================================================
//  jdm_ui.c — JDM UI 入口实现
//
//  boot 序列:splash(face 底 + "SKY GAUGE")停 ~1.2s(顺带给字体
//  预热让路)→ 16 页轮播(原型 PAGES 顺序)。
// ================================================================

#include "jdm_ui.h"

#include "esp_log.h"
#include "lvgl.h"

#include "core/jdm_fonts.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_theme.h"
#include "pages/pages.h"
#include "bsp_obd_dsp/nvs_storage.h"

static const char *TAG = "jdm_ui";

const jdm_page_t *const jdm_pages[] = {
    &page_gear, &page_boost, &page_coolant, &page_oiltemp,
    &page_oilpress, &page_triple, &page_multi, &page_tyres,
    &page_speed, &page_gforce, &page_heart, &page_analog,
    &page_music, &page_display, &page_bluetooth, &page_pair,
};
const uint8_t jdm_page_count = (uint8_t)(sizeof(jdm_pages) / sizeof(jdm_pages[0]));

// ----------------------------------------------------------------
//  splash(非原型页,仅开机 1.2s;后续视需要并入开机动画)
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

static void boot_done_cb(lv_timer_t *t)
{
    lv_timer_del(t);
    jdm_nav_init();
    jdm_nav_set_pages(jdm_pages, jdm_page_count);
    ESP_LOGI(TAG, "nav up (%u pages)", jdm_page_count);
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
