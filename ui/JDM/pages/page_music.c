// ================================================================
//  page_music.c — 音乐 / 歌词(原型 13 页 NOW PLAYING)
//
//  固件无媒体源:恒"SOURCE UNAVAILABLE"态(唱片 + 标题占位 +
//  提示文案 + 进度条空)。按钮以线稿符号呈现,当前不可用。
//  原型歌词为中文——CJK 字体未嵌入,提示文案用英文(已知偏差)。
// ================================================================

#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"

static lv_obj_t *s_hint;

static lv_obj_t *circle_create(lv_obj_t *parent, int32_t mcx, int32_t mcy,
                               int32_t mr, bool filled)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    int32_t d = UIS(2 * mr);
    lv_obj_set_size(c, d, d);
    lv_obj_set_pos(c, UIS(mcx) - d / 2, UIS(mcy) - d / 2);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    if (filled) {
        lv_obj_set_style_bg_color(c, jdm_color(JDM_ROLE_PANEL), 0);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_border_color(c, jdm_color(JDM_ROLE_LINE), 0);
        lv_obj_set_style_border_width(c, UIS(2), 0);
    }
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return c;
}

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "NOW PLAYING", "MEDIA / UNAVAILABLE");

    // 唱片(同心圆)
    circle_create(page, 174, 284, 68, true);
    circle_create(page, 174, 284, 52, false);
    circle_create(page, 174, 284, 35, false);
    lv_obj_t *dot = circle_create(page, 174, 284, 12, true);
    lv_obj_set_style_bg_color(dot, jdm_color(JDM_ROLE_MUTED), 0);

    jdm_text(page, 280, 233, "No media", JDM_FONT_MEDIA_TITLE, JDM_ROLE_TEXT, JDM_TEXT_START);
    jdm_text(page, 280, 305, "SOURCE UNAVAILABLE", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);

    s_hint = jdm_text(page, 360, 399, "CONNECT A MEDIA DEVICE",
                      JDM_FONT_BODY, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);

    // 进度条(空)
    lv_obj_t *bar = lv_obj_create(page);
    lv_obj_remove_style_all(bar);
    lv_obj_set_pos(bar, UIS(150), UIS(480));
    lv_obj_set_size(bar, UIS(420), UIS(6));
    lv_obj_set_style_radius(bar, UIS(3), 0);
    lv_obj_set_style_bg_color(bar, jdm_color(JDM_ROLE_PANEL), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    jdm_text(page, 150, 498, "\xE2\x80\x94:\xE2\x80\x94", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_START);
    jdm_text(page, 570, 498, "\xE2\x80\x94:\xE2\x80\x94", JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_END);

    // 三键线稿符号(上一首/播放/下一首,均不可用态)
    static const struct { int32_t cx; } keys[3] = {{248, 0}, {360, 0}, {476, 0}};
    for (int i = 0; i < 3; i++) {
        circle_create(page, keys[i].cx, 584, 43, true);
    }
    // prev / next 符号(线 + 三角轮廓)
    static const jdm_pt_t prev1[] = {{230, 567}, {230, 601}};
    jdm_draw_poly(page, prev1, 2, 3, JDM_ROLE_MUTED);
    static const jdm_pt_t prev2[] = {{276, 567}, {236, 584}, {276, 601}, {276, 567}};
    jdm_draw_poly(page, prev2, 4, 3, JDM_ROLE_MUTED);
    static const jdm_pt_t play[] = {{348, 565}, {380, 584}, {348, 603}, {348, 565}};
    jdm_draw_poly(page, play, 4, 4, JDM_ROLE_MUTED);
    static const jdm_pt_t next1[] = {{490, 567}, {490, 601}};
    jdm_draw_poly(page, next1, 2, 3, JDM_ROLE_MUTED);
    static const jdm_pt_t next2[] = {{444, 567}, {484, 584}, {444, 601}, {444, 567}};
    jdm_draw_poly(page, next2, 4, 3, JDM_ROLE_MUTED);
}

static void update(void)
{
    // 无媒体源;数据源(如 AVRCP)接入后在此刷新
}

const jdm_page_t page_music = {"music", create, update, 200};
