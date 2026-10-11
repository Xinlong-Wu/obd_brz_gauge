#pragma once
// ================================================================
//  jdm_fonts_logic.h — JDM 字体表纯逻辑(static inline,零 LVGL 依赖)
//
//  角色表 = ui-prototype/foundations.js 字号阶梯(母版 px);
//  家族 = 4 个 TTF(Barlow Condensed 500/600 + IBM Plex Mono 400/500)。
//  tests/ 直接编译断言(见 test_jdm_fonts.c)。
// ================================================================

#include <stdint.h>

#include "core/jdm_geo.h"   // UI_MASTER_RES(纯宏)

/** 字体家族 = 嵌入的 TTF blob(生成物 assets/fonts/jdm_*.h)。 */
typedef enum {
    JDM_FAM_BARLOW = 0,      // Barlow Condensed 500(数字默认)
    JDM_FAM_BARLOW_SEMI,     // Barlow Condensed 600(配对码/歌名)
    JDM_FAM_MONO,            // IBM Plex Mono 400(小字/副标题)
    JDM_FAM_MONO_MED,        // IBM Plex Mono 500(标签/单位/状态)
    JDM_FAM_COUNT,
} jdm_font_family_t;

/** 字体角色 = 设计系统字号阶梯的一员。 */
typedef enum {
    JDM_FONT_HERO = 0,       // 180 primary 读数主值
    JDM_FONT_ECCENTRIC,      // 144 偏心读数(油压页)
    JDM_FONT_ANALOG,         // 96  机械表读数
    JDM_FONT_BOOST,          // 90  multi 页中央大值
    JDM_FONT_TYRE,           // 80  胎压数字
    JDM_FONT_COMPACT,        // 76  compact 读数
    JDM_FONT_STATISTIC,      // 50  heart 页三统计值
    JDM_FONT_PAIR,           // 82  配对码(600)
    JDM_FONT_MEDIA_TITLE,    // 60  歌名(600)
    JDM_FONT_BODY,           // 30  正文/当前歌词
    JDM_FONT_SMALL,          // 22  副标题/chip/刻度数字
    JDM_FONT_TITLE,          // 28  页标题
    JDM_FONT_LABEL,          // 26  readout 标签/按钮文字
    JDM_FONT_SUPPORT,        // 24  单位/状态字
    JDM_FONT_COUNT,
} jdm_font_role_t;

typedef struct {
    uint8_t family;      // jdm_font_family_t
    uint16_t master_px;  // 720 母版字号
} jdm_font_spec_t;

extern const jdm_font_spec_t JDM_FONT_SPECS[JDM_FONT_COUNT];

/** 母版字号 → 渲染分辨率字号(对称取整,与 UIS() 同约定;入参恒为正)。 */
static inline int jdm_font_px(int master_px, int render_res)
{
    return (master_px * render_res + (UI_MASTER_RES / 2)) / UI_MASTER_RES;
}
