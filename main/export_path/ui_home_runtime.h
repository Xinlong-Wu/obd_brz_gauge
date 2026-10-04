#pragma once
// ================================================================
//  ui_home_runtime.h — 用户自定义仪表页运行时(M4.b)
//
//  首页 = MENU 平铺 + N 个仪表页平铺 + ADD 平铺,左右滑切换;
//  仪表页按槽位模型渲染内置组件(1/2/3/4/5/6 槽行布局),数据走
//  统一通道。本增量先经模拟器 --home 预览验收;接管固件导航与
//  编辑态(长按 EDIT/DELETE/BACK)在下一增量完成。
// ================================================================

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 构建首页(挂到当前默认显示),按 NVS dashboard 配置渲染平铺。
 *  返回首页根对象(兼作手势目标)。 */
lv_obj_t *ui_home_init(void);

/** 当前平铺索引(0=MENU .. N+1=ADD)。 */
uint8_t ui_home_active_tile(void);

/** 滑到相邻平铺(dir:左/右);越界不动。返回是否切换。 */
bool ui_home_step(int dir);

/** 子页面"返回 home"手势处理(左右滑;ui_dashboard_config 挂接)。 */
void ui_event_home_return(lv_event_t *e);

/** 取首页根对象(不存在则构建);供其它页面返回导航使用。 */
lv_obj_t *ui_home_get(void);

/** 数据刷新(由宿主定时器驱动;组件自读缓存)。 */
void ui_home_refresh(void);

#ifdef __cplusplus
}
#endif
