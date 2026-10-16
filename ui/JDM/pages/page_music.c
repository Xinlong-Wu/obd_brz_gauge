// ================================================================
//  page_music.c — 音乐 / 歌词(原型 13 页 NOW PLAYING)
//
//  固件无媒体源:恒"SOURCE UNAVAILABLE"态(唱片 + 标题占位 +
//  提示文案 + 进度条空)。按键按原型 mediaButton 几何呈现,整组
//  禁用态(40% 不透明度)。
//  原型歌词为中文——CJK 字体未嵌入,提示文案用英文(已知偏差)。
// ================================================================

#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_draw.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"

static lv_obj_t *s_hint;

// play 填充三角(TEXT 色;随整组禁用透明度,见 keys 容器 opa)
static void play_fill_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    // UIS() 按渲染分辨率折算,sim 240/466 档为运行时值,不能静态初始化
    const lv_point_precise_t pts[3] = {
        {UIS(348), UIS(569)}, {UIS(380), UIS(588)}, {UIS(348), UIS(607)},
    };
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.p[0] = pts[0];
    d.p[1] = pts[1];
    d.p[2] = pts[2];
    d.color = jdm_color(JDM_ROLE_TEXT);
    d.opa = LV_OPA_COVER;   // 容器 kc 的 opa 已统一打折
    lv_draw_triangle(layer, &d);
}

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

    jdm_text(page, 280, 233 + 6 - jdm_text_glyph_top_off(jdm_font(JDM_FONT_MEDIA_TITLE)), "No media", JDM_FONT_MEDIA_TITLE, JDM_ROLE_TEXT, JDM_TEXT_START);
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

    // 三键(prev r43 / 播放主键 r51+amber 细圈 / next r43;y=588):
    // 原型 mediaButton()——play 符号填充暖白,prev/next 描边;无媒体源
    // 时整组按禁用态呈现(40% 不透明度)
    lv_obj_t *kc = lv_obj_create(page);
    lv_obj_remove_style_all(kc);
    lv_obj_set_size(kc, UIS(720), UIS(720));
    lv_obj_set_pos(kc, 0, 0);
    lv_obj_clear_flag(kc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_opa(kc, JDM_CTL_DISABLED_OPA, 0);

    circle_create(kc, 248, 588, 43, true);
    lv_obj_t *play = circle_create(kc, 360, 588, JDM_CTL_MUSIC_RADIUS, true);
    lv_obj_set_style_border_color(play, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_border_width(play, UIS(2), 0);
    circle_create(kc, 472, 588, 43, true);
    lv_obj_add_event_cb(kc, play_fill_cb, LV_EVENT_DRAW_MAIN, NULL);

    // prev / next 符号(竖线 + 三角轮廓,TEXT 色)
    static const jdm_pt_t prev1[] = {{230, 571}, {230, 605}};
    jdm_draw_poly(kc, prev1, 2, 3, JDM_ROLE_TEXT);
    static const jdm_pt_t prev2[] = {{276, 571}, {236, 588}, {276, 605}, {276, 571}};
    jdm_draw_poly(kc, prev2, 4, 3, JDM_ROLE_TEXT);
    static const jdm_pt_t next1[] = {{490, 571}, {490, 605}};
    jdm_draw_poly(kc, next1, 2, 3, JDM_ROLE_TEXT);
    static const jdm_pt_t next2[] = {{444, 571}, {484, 588}, {444, 605}, {444, 571}};
    jdm_draw_poly(kc, next2, 4, 3, JDM_ROLE_TEXT);
}

static void update(void)
{
    // 无媒体源;数据源(如 AVRCP)接入后在此刷新
}

const jdm_page_t page_music = {"music", create, update, 200};
