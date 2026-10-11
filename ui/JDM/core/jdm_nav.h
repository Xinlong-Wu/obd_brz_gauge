#pragma once
// ================================================================
//  jdm_nav.h — JDM 页面导航(16 页横向轮播)
//
//  一屏一页:page.create 把整页控件建进根容器(含 shell),update 由
//  导航的 lv_timer 按页周期驱动,切页即销毁重建(页面不驻留,内存
//  峰值 = 最重一页)。左右滑动切页(LV_EVENT_GESTURE)。
// ================================================================

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;          // 调试标识
    void (*create)(lv_obj_t *parent);  // 整页构建(parent = 全屏容器)
    void (*update)(void);      // 周期数据刷新(可空)
    uint32_t update_period_ms; // 刷新周期;0 = 默认 33ms(指针页给 16)
} jdm_page_t;

#define JDM_NAV_UPDATE_PERIOD_DEFAULT 33

/** 初始化导航(建根容器 + 手势 + 刷新 timer),此后 set_pages。 */
void jdm_nav_init(void);

/** 注册轮播页表(jdm_page_t 指针数组,须常驻)。自动 goto(0)。 */
void jdm_nav_set_pages(const jdm_page_t *const *pages, uint8_t count);

/** 跳页(环形取模)。 */
void jdm_nav_goto(uint8_t idx);

/** 步进一页(dir: +1 左滑下一页,−1 右滑上一页)。返回是否切换。 */
bool jdm_nav_step(int dir);

/** 当前页索引。 */
uint8_t jdm_nav_active(void);

#ifdef __cplusplus
}
#endif
