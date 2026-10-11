// ================================================================
//  jdm_theme.c — JDM 主题运行时实现
// ================================================================

#include "jdm_theme.h"
#include "jdm_tokens.h"

#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/boards/board_display_compat.h"   // Set_Backlight()

#include <stdatomic.h>

static atomic_bool s_night;

static const uint32_t s_palette[JDM_ROLE_COUNT] = {
    [JDM_ROLE_CANVAS] = JDM_HEX_CANVAS,
    [JDM_ROLE_FACE]   = JDM_HEX_FACE,
    [JDM_ROLE_PANEL]  = JDM_HEX_PANEL,
    [JDM_ROLE_LINE]   = JDM_HEX_LINE,
    [JDM_ROLE_TEXT]   = JDM_HEX_TEXT,
    [JDM_ROLE_MUTED]  = JDM_HEX_MUTED,
    [JDM_ROLE_AMBER]  = JDM_HEX_AMBER,
    [JDM_ROLE_RED]    = JDM_HEX_RED,
    [JDM_ROLE_GREEN]  = JDM_HEX_GREEN,
    [JDM_ROLE_BLUE]   = JDM_HEX_BLUE,
};

lv_color_t jdm_color(jdm_color_role_t role)
{
    if ((int)role < 0 || (int)role >= (int)JDM_ROLE_COUNT) {
        return lv_color_hex(JDM_HEX_RED);   // 编码错误显眼化
    }
    if (role == JDM_ROLE_TEXT && atomic_load(&s_night)) {
        return lv_color_hex(JDM_HEX_NIGHT);
    }
    return lv_color_hex(s_palette[role]);
}

bool jdm_theme_is_night(void)
{
    return atomic_load(&s_night);
}

static uint8_t clamp_brightness(int v)
{
    if (v < 10) return 10;
    if (v > 100) return 100;
    return (uint8_t)v;
}

void jdm_theme_apply_brightness(void)
{
    const nvs_user_cfg_t *cfg = nvs_cfg_get();
    uint8_t pct = jdm_theme_is_night()
                      ? clamp_brightness(cfg->brightness_night ? cfg->brightness_night : 35)
                      : clamp_brightness(cfg->brightness_day ? cfg->brightness_day : 85);
    Set_Backlight(pct);
}

void jdm_theme_set_night(bool night)
{
    atomic_store(&s_night, night);
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    if (cfg->night_mode != (uint8_t)night) {
        cfg->night_mode = (uint8_t)night;
        nvs_cfg_set(cfg);
    }
    jdm_theme_apply_brightness();
}

uint8_t jdm_theme_adjust_brightness(bool night, int delta)
{
    nvs_user_cfg_t *cfg = (nvs_user_cfg_t *)nvs_cfg_get();
    int cur = night ? (cfg->brightness_night ? cfg->brightness_night : 35)
                    : (cfg->brightness_day ? cfg->brightness_day : 85);
    cur = clamp_brightness(cur + delta);
    if (night) {
        cfg->brightness_night = (uint8_t)cur;
    } else {
        cfg->brightness_day = (uint8_t)cur;
    }
    nvs_cfg_set(cfg);
    if (jdm_theme_is_night() == night) {
        jdm_theme_apply_brightness();   // 只在调的是当前档时即时生效
    }
    return (uint8_t)cur;
}
