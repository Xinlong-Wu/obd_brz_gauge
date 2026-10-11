#pragma once
// ================================================================
//  jdm_theme.h — JDM 主题运行时(日/夜 + 取色)
//
//  JDM 只有一套深色外观;唯一"主题态"是夜间模式:TEXT 角色被
//  NIGHT(#E8B27A)接管(对应原型把所有 #F3F0E7 换成 night 色),
//  背光亮度按日/夜两档记忆(NVS brightness_day / brightness_night)。
//  组件不缓存颜色,每帧经 jdm_color() 现取——切日夜即全局生效。
// ================================================================

#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 语义色角色(tokens.h 里 11 个语义色去掉 night;night 只经 TEXT 接管)。 */
typedef enum {
    JDM_ROLE_CANVAS = 0,
    JDM_ROLE_FACE,
    JDM_ROLE_PANEL,
    JDM_ROLE_LINE,
    JDM_ROLE_TEXT,
    JDM_ROLE_MUTED,
    JDM_ROLE_AMBER,
    JDM_ROLE_RED,
    JDM_ROLE_GREEN,
    JDM_ROLE_BLUE,
    JDM_ROLE_COUNT,
} jdm_color_role_t;

/** 取语义色。夜间模式下 JDM_ROLE_TEXT 返回 night 色,其余不变。 */
lv_color_t jdm_color(jdm_color_role_t role);

/** 当前是否夜间模式。 */
bool jdm_theme_is_night(void);

/** 切换日/夜并持久化(NVS night_mode)+ 应用背光。 */
void jdm_theme_set_night(bool night);

/** 按当前日/夜档把背光刷成 NVS 记忆值(开机与切档时调用)。 */
void jdm_theme_apply_brightness(void);

/** 亮度调整(display 页 ±5;越界钳 10..100,写回 NVS 并即时生效)。
 *  night=false 调日间档,true 调夜间档。返回调整后的值。 */
uint8_t jdm_theme_adjust_brightness(bool night, int delta);

#ifdef __cplusplus
}
#endif
