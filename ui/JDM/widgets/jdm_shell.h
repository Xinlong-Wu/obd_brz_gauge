#pragma once
// ================================================================
//  jdm_shell.h — 表盘底座(每页共用的 shell)
//
//  对应原型 components.js shell():face 底色圆(r=360)、刻度环
//  (r=342,line 色 2px)、顶部定位标记(amber 小圆角矩形)。
//  内容容器在 face 圆之上、刻度环之下,页面控件全部建进它。
// ================================================================

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *content;   // 页面内容父容器(全屏,已在 shell 层之上)
} jdm_shell_t;

/** 在 parent(页容器)上建 shell。返回 content 指针(直接往里建控件)。 */
lv_obj_t *jdm_shell_build(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
