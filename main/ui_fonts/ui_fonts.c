/* ui_fonts.c — FontTypoder 八档字体的 TinyTTF 运行时实现。
 *
 * 替代旧 lv_font_conv 位图管线:Conthrax OTF 原文嵌在 contrax_otf.c
 * (flash .rodata,由 tools/gen_font_blob.py 生成),各档在 init 时按渲染
 * 分辨率定 px 后创建;字形首次绘制时栅格化,进 per-font LRU 缓存
 * (LV_TINY_TTF_CACHE_GLYPH_CNT,默认 128)。
 * 大小写不敏感的常识约束:符号名沿用 SquareLine 时代的 Typoder 命名,
 * ~110 处 &ui_font_X 调用点因此零改动。
 */
#include "ui_fonts.h"

#include <string.h>

#include "esp_log.h"

#include "../export_path/ui.h"
#include "ui_fonts_logic.h"
#include "conthrax_otf.h"

static const char *TAG = "ui_fonts";

lv_font_t ui_font_FontTypoderSize16;
lv_font_t ui_font_FontTypoderSize20;
lv_font_t ui_font_FontTypoderSize24;
lv_font_t ui_font_FontTypoderSize36;
lv_font_t ui_font_FontTypoderSize40;
lv_font_t ui_font_FontTypoderSize44;
lv_font_t ui_font_FontTypoderSize56;
lv_font_t ui_font_FontTypoderSize140;

void ui_fonts_init(int render_res)
{
    /* 槽位顺序与 ui_fonts_logic.h 的 nominal 表一一对应 */
    static lv_font_t *const slots[UI_FONT_COUNT] = {
        &ui_font_FontTypoderSize16,  &ui_font_FontTypoderSize20,
        &ui_font_FontTypoderSize24,  &ui_font_FontTypoderSize36,
        &ui_font_FontTypoderSize40,  &ui_font_FontTypoderSize44,
        &ui_font_FontTypoderSize56,  &ui_font_FontTypoderSize140,
    };

    for (int i = 0; i < UI_FONT_COUNT; i++) {
        int px = ui_font_px(ui_font_nominal_sizes[i], render_res);
        lv_font_t *f = lv_tiny_ttf_create_data(ui_conthrax_otf_data,
                                               UI_CONTHRAX_OTF_SIZE, px);
        if (f == NULL) {
            /* OTF 随固件入库,走到这里只能是 LVGL 配置问题;退默认字保命 */
            ESP_LOGE(TAG, "tiny_ttf create %dpx failed, falling back to default font", px);
            *slots[i] = *LV_FONT_DEFAULT;
            continue;
        }
        *slots[i] = *f;
        ESP_LOGI(TAG, "FontTypoderSize%d -> %dpx", ui_font_nominal_sizes[i], px);
    }
}

void ui_fonts_prewarm_size140(void)
{
    /* 齿轮页 16ms 刷新,大字首次栅格化在 240MHz 上是几十 ms 的一帧顿挫;
     * Logo 停留期间把这 12 个字形做进缓存,首切就是纯 blit。
     * release 后条目仍留在 LRU 里,后续绘制直接命中。 */
    static const char glyphs[] = "0123456789NR";
    for (unsigned i = 0; glyphs[i] != '\0'; i++) {
        lv_font_glyph_dsc_t dsc;
        if (!lv_font_get_glyph_dsc(&ui_font_FontTypoderSize140, &dsc, glyphs[i], 0)) {
            continue;
        }
        lv_font_get_glyph_bitmap(&dsc, NULL);
        lv_font_glyph_release_draw_data(&dsc);
    }
}
