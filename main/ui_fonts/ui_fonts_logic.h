/* ui_fonts_logic.h — 字体字号纯逻辑(无 LVGL / ESP-IDF 依赖) */
#pragma once
#include <stdint.h>

/*
 * 沿用 720 母版时代的字号换算:名义字号(SquareLine 360 屏时代命名)× 2
 * 得母版 px,再按渲染分辨率等比落档,下限 8px。与旧 gen_fonts.py 的
 * target_px() 逐值一致(含四舍五入),保证切换 TinyTTF 后各档视觉大小不变。
 */

#define UI_FONT_COUNT 8

/* 名义字号档位,顺序即 ui_font_FontTypoderSize* 的声明顺序 */
static const uint8_t ui_font_nominal_sizes[UI_FONT_COUNT] = {16, 20, 24, 36, 40, 44, 56, 140};

/** 名义字号 + 渲染分辨率(240/360/466) → 实际栅格化 px(四舍五入,下限 8)。 */
static inline int ui_font_px(int nominal, int render_res)
{
    /* 整数版 round(master*res/720):×2 再加半个分母,与 Python int(x+0.5) 等价 */
    int px = (nominal * 2 * render_res * 2 + 720) / 1440;
    return px < 8 ? 8 : px;
}
