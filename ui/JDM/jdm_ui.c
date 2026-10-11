// ================================================================
//  jdm_ui.c — JDM UI 入口实现
//
//  boot 序列:splash(face 底 + "SKY GAUGE")停 ~1.2s(顺带给字体
//  预热让路)→ 建 16 页轮播。页面在 pages/ 逐个落地;此处先挂
//  骨架占位页,保证任何时刻固件可构建。
// ================================================================

#include "jdm_ui.h"

#include "esp_log.h"
#include "lvgl.h"

#include "core/jdm_fonts.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_shell.h"
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
//  首页占位(M0 骨架;M2 起逐页替换为 pages/ 真页)
// ----------------------------------------------------------------

static void placeholder_create(lv_obj_t *parent)
{
    lv_obj_t *content = jdm_shell_build(parent);

    lv_obj_t *title = lv_label_create(content);
    lv_obj_set_style_text_font(title, jdm_font(JDM_FONT_TITLE), 0);
    lv_obj_set_style_text_color(title, jdm_color(JDM_ROLE_AMBER), 0);
    lv_label_set_text(title, "JDM");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UIS(78));

    lv_obj_t *sub = lv_label_create(content);
    lv_obj_set_style_text_font(sub, jdm_font(JDM_FONT_SMALL), 0);
    lv_obj_set_style_text_color(sub, jdm_color(JDM_ROLE_MUTED), 0);
    lv_label_set_text(sub, "SKELETON BUILD");
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, UIS(122));

    lv_obj_t *hint = lv_label_create(content);
    lv_obj_set_style_text_font(hint, jdm_font(JDM_FONT_SMALL), 0);
    lv_obj_set_style_text_color(hint, jdm_color(JDM_ROLE_MUTED), 0);
    lv_label_set_text(hint, "SWIPE");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -UIS(60));
}

static void placeholder_update(void)
{
}

static const jdm_page_t s_pages[] = {
    {"placeholder", placeholder_create, placeholder_update, 200},
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
