#pragma once
// ================================================================
//  jdm_tokens.h — JDM 设计 token(单一事实源)
//
//  对应 ui-prototype/foundations.js:11 个语义色 + 夜间接管色、
//  几何常量、间距、描边、控件规格。色值只在这里出现一次;
//  代码一律经 jdm_color(角色) 取色,不直接引用宏。
// ================================================================

// ---- 调色板(foundations.js palette)----
#define JDM_HEX_CANVAS  0x141718   // 外页背景(非表盘)
#define JDM_HEX_FACE    0x090D0F   // 表盘底色
#define JDM_HEX_PANEL   0x151C1F   // 控件表面/轨道/车体
#define JDM_HEX_LINE    0x334046   // 结构线/未点亮刻度/环
#define JDM_HEX_TEXT    0xF3F0E7   // 主要读数(暖白)
#define JDM_HEX_MUTED   0xA6B0AD   // 辅助文字/单位/标签
#define JDM_HEX_AMBER   0xFFB35B   // 选中/强调/advisory
#define JDM_HEX_RED     0xFF4D4D   // warning/critical
#define JDM_HEX_GREEN   0x06D6A0   // 正常状态
#define JDM_HEX_BLUE    0x91C6DB   // 连接信息
#define JDM_HEX_NIGHT   0xE8B27A   // 夜间接管 TEXT

// ---- 语义色角色(取色唯一入口 jdm_color();night 只经 TEXT 接管) ----
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

// ---- 几何(母版 px)----
#define JDM_GEO_MASTER       720
#define JDM_GEO_RADIUS       360    // 整表半径(内容裁切边界)
#define JDM_GEO_RIM_RADIUS   342    // 刻度环半径
#define JDM_GEO_ALERT_RADIUS 330    // critical 外缘弧半径
#define JDM_GEO_INTERACTIVE_RADIUS 318  // 触摸安全区(控件四角约束)

// ---- 间距 ----
#define JDM_SPACE_XS  8
#define JDM_SPACE_SM  16
#define JDM_SPACE_MD  24
#define JDM_SPACE_LG  32

// ---- 描边 ----
#define JDM_STROKE_HAIRLINE 2
#define JDM_STROKE_NEEDLE   5

// ---- 控件 ----
#define JDM_CTL_HIT_MIN      88     // 最小点击区
#define JDM_CTL_ACTION_H     88     // 按钮高(44 显示 + 上下 hit)
#define JDM_CTL_MUSIC_HIT    104
#define JDM_CTL_MUSIC_RADIUS 51
#define JDM_CTL_ROUND_RADIUS 43
#define JDM_CTL_DISABLED_OPA 102    // 0.4 × 255

// ---- 报警 ----
#define JDM_ALERT_RIM_WIDTH  4
#define JDM_ALERT_FOOTER_Y   638    // 底部报警摘要 y(母版)

// ---- 报警等级(alertInk:advisory→amber,warning/critical→red)----
typedef enum {
    JDM_ALERT_NONE = 0,
    JDM_ALERT_ADVISORY,
    JDM_ALERT_WARNING,
    JDM_ALERT_CRITICAL,
} jdm_alert_level_t;
