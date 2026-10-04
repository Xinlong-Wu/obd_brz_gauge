// ================================================================
//  stub_ui_theme.c — 编译期主题访问桩(单测专用)
//
//  真实的 ui_theme.c/ui_theme_generated.c 依赖 gen_themes.py 生成的
//  theme_assets/*.c(不入库),单测不应引入 Pillow/代码gen依赖。
//  这里返回 default 主题(themes/builtin/default/theme.toml)的固定
//  调色板,只服务 theme_loader.c 的 theme_load_default() 回退路径。
//  真实 ui_theme 在固件与模拟器构建里覆盖。
// ================================================================

#include "export_path/ui_theme.h"

#include <stddef.h>

static const ui_theme_t s_stub_default = {
    .id = "default",
    .name = "DEFAULT",
    .colors = {
        [UI_COLOR_BG] = 0x000000,
        [UI_COLOR_RING] = 0xFFFFFF,
        [UI_COLOR_ARC_TRACK] = 0x333333,
        [UI_COLOR_ARC_INDICATOR] = 0xFFFFFF,
        [UI_COLOR_TEXT_PRIMARY] = 0xFFFFFF,
        [UI_COLOR_TEXT_SECONDARY] = 0x888888,
        [UI_COLOR_NEEDLE] = 0xFF1010,
        [UI_COLOR_PANEL] = 0x222222,
    },
};

const ui_theme_t *ui_theme_get(uint8_t idx)
{
    (void)idx;
    return &s_stub_default;
}

const ui_theme_t *ui_theme_active(void)
{
    return &s_stub_default;
}

lv_color_t ui_theme_color_lv(ui_color_role_t role)
{
    return lv_color_hex(s_stub_default.colors[role]);
}
