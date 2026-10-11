// ================================================================
//  jdm_fonts.c — JDM 字体实现(TinyTTF 运行时)
//
//  四个 TTF 原文嵌在 assets/fonts/(tools/gen_jdm_fonts.py 生成,
//  源 = ui-prototype/fonts,SIL OFL 许可随目录入库)。各角色在 init 时
//  按渲染分辨率定 px 后创建;字形首次绘制时栅格化,进 per-font LRU 缓存。
// ================================================================

#include "jdm_fonts.h"

#include <string.h>

#include "esp_log.h"

#include "assets/fonts/jdm_barlow_500.h"
#include "assets/fonts/jdm_barlow_600.h"
#include "assets/fonts/jdm_plex_mono_400.h"
#include "assets/fonts/jdm_plex_mono_500.h"

static const char *TAG = "jdm_fonts";

const jdm_font_spec_t JDM_FONT_SPECS[JDM_FONT_COUNT] = {
    [JDM_FONT_HERO]       = {JDM_FAM_BARLOW,      180},
    [JDM_FONT_ECCENTRIC]  = {JDM_FAM_BARLOW,      144},
    [JDM_FONT_ANALOG]     = {JDM_FAM_BARLOW,       96},
    [JDM_FONT_BOOST]      = {JDM_FAM_BARLOW,       90},
    [JDM_FONT_TYRE]       = {JDM_FAM_BARLOW,       80},
    [JDM_FONT_COMPACT]    = {JDM_FAM_BARLOW,       76},
    [JDM_FONT_STATISTIC]  = {JDM_FAM_BARLOW,       50},
    [JDM_FONT_PAIR]       = {JDM_FAM_BARLOW_SEMI,  82},
    [JDM_FONT_MEDIA_TITLE]= {JDM_FAM_BARLOW_SEMI,  60},
    [JDM_FONT_BODY]       = {JDM_FAM_MONO,         30},
    [JDM_FONT_SMALL]      = {JDM_FAM_MONO,         22},
    [JDM_FONT_TITLE]      = {JDM_FAM_MONO_MED,     28},
    [JDM_FONT_LABEL]      = {JDM_FAM_MONO_MED,     26},
    [JDM_FONT_SUPPORT]    = {JDM_FAM_MONO_MED,     24},
};

static const struct {
    const uint8_t *data;
    int size;
} s_family_blob[JDM_FAM_COUNT] = {
    [JDM_FAM_BARLOW]      = {jdm_barlow_500_data,      JDM_BARLOW_500_SIZE},
    [JDM_FAM_BARLOW_SEMI] = {jdm_barlow_600_data,      JDM_BARLOW_600_SIZE},
    [JDM_FAM_MONO]        = {jdm_plex_mono_400_data,   JDM_PLEX_MONO_400_SIZE},
    [JDM_FAM_MONO_MED]    = {jdm_plex_mono_500_data,   JDM_PLEX_MONO_500_SIZE},
};

static lv_font_t s_fonts[JDM_FONT_COUNT];
static bool s_inited;

const lv_font_t *jdm_font(jdm_font_role_t role)
{
    if (!s_inited || (int)role < 0 || (int)role >= (int)JDM_FONT_COUNT) {
        return LV_FONT_DEFAULT;
    }
    return &s_fonts[role];
}

void jdm_fonts_init(int render_res)
{
    for (int i = 0; i < JDM_FONT_COUNT; i++) {
        int px = jdm_font_px(JDM_FONT_SPECS[i].master_px, render_res);
        lv_font_t *f = lv_tiny_ttf_create_data(s_family_blob[JDM_FONT_SPECS[i].family].data,
                                               s_family_blob[JDM_FONT_SPECS[i].family].size, px);
        if (f == NULL) {
            /* TTF 随固件入库,走到这里只能是 LVGL 配置问题;退默认字保命 */
            ESP_LOGE(TAG, "tiny_ttf create role %d %dpx failed, fallback", i, px);
            s_fonts[i] = *LV_FONT_DEFAULT;
            continue;
        }
        s_fonts[i] = *f;
    }
    s_inited = true;
}

void jdm_fonts_prewarm(void)
{
    /* 首页 hero 大字 0-9 首次栅格化是几十 ms 的一帧顿挫;
     * splash 停留期间做进缓存,首切就是纯 blit。 */
    static const char glyphs[] = "0123456789";
    const lv_font_t *hero = jdm_font(JDM_FONT_HERO);
    for (unsigned i = 0; glyphs[i] != '\0'; i++) {
        lv_font_glyph_dsc_t dsc;
        if (!lv_font_get_glyph_dsc(hero, &dsc, glyphs[i], 0)) {
            continue;
        }
        lv_font_get_glyph_bitmap(&dsc, NULL);
        lv_font_glyph_release_draw_data(&dsc);
    }
}
