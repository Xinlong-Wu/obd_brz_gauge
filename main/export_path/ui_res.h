#pragma once
// ================================================================
//  ui_res.h — 分辨率无关布局(P1)
//
//  布局/UI/素材按 UI_MASTER_RES(720) 母版书写,编译期经 UIS() 折叠到
//  目标渲染分辨率(CONFIG_OBD_UI_RENDER_RES,Kconfig,默认 360):
//    UIS(720) @360=360 @240=240 @466=466   —— 全屏/圆角/直径类
//    UIS(680) @360=340 @240=226 @466=440   —— 表盘弧等
//  规则:
//    - 只包裹"几何像素"(尺寸/坐标/圆角/内边距/线宽),角度/透明度/
//      延时/计数永远不裹
//    - 迁移由 tools/migrate_ui_literals.py 完成,新代码手写母版值
//    - sim/tests 无 sdkconfig.h 时默认 360(可 -DUI_RENDER_RES 覆盖)
//  运行时按 720/1080 渲染再下采样的方案已被否决(闪存/PSRAM/帧率,
//  见 docs/DEVELOPMENT.md 分辨率策略)。
// ================================================================

#ifndef UI_RENDER_RES
#ifdef CONFIG_OBD_UI_RENDER_RES
#define UI_RENDER_RES CONFIG_OBD_UI_RENDER_RES
#else
#define UI_RENDER_RES 360   /* simulator / unit tests 默认 */
#endif
#endif

#define UI_MASTER_RES 720

/** 母版像素 → 目标渲染像素(编译期整数,对称四舍五入:负值远离零取整,
 *  否则 -142 这类偏移在 360 下会偏 1px,sim 金图失配)。 */
#define UIS(px) (((int32_t)(px) >= 0) \
                 ? (((int32_t)(px) * UI_RENDER_RES + (UI_MASTER_RES / 2)) / UI_MASTER_RES) \
                 : (((int32_t)(px) * UI_RENDER_RES - (UI_MASTER_RES / 2)) / UI_MASTER_RES))
