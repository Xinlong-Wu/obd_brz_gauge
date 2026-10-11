#pragma once
// ================================================================
//  jdm_ui.h — JDM UI 入口
//
//  替代旧 ui_init()/ui_ext_init()。app_main 在 LVGL 任务锁内调用
//  一次 jdm_ui_init():建 splash(约 1s)→ 16 页轮播导航。
//  跨任务数据照旧走 obd_data_cache / app_event,无直接 UI 调用。
// ================================================================

#ifdef __cplusplus
extern "C" {
#endif

/** 构建并显示 JDM UI(splash → 轮播首页)。须持 LVGL 锁调用。
 *  @param render_res 渲染分辨率:固件传 UI_RENDER_RES,模拟器传运行时
 *         --ui-res 值(字体按它落档)。 */
void jdm_ui_init(int render_res);

#ifdef __cplusplus
}
#endif
