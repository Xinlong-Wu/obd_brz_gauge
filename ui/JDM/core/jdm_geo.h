#pragma once
// ================================================================
//  jdm_geo.h — 分辨率无关布局
//
//  JDM UI 按 UI_MASTER_RES(720) 母版坐标书写(ui-prototype 同一坐标系,
//  显示参考 360,所有母版值 ÷2 即 360 屏像素),编译期经 UIS() 折叠到
//  目标渲染分辨率(CONFIG_OBD_UI_RENDER_RES,Kconfig,默认 360):
//    UIS(720) @360=360 @240=240 @466=466   —— 全屏/圆角/直径类
//    UIS(684) @360=342 @240=228 @466=442   —— 表盘刻度环(r=342)
//  规则:
//    - 只包裹"几何像素"(尺寸/坐标/圆角/内边距/线宽),角度/透明度/
//      延时/计数永远不裹
//    - sim/tests 无 sdkconfig.h 时默认 360(可 -DUI_RENDER_RES 覆盖)
// ================================================================

// 本文件被固件/sim/主机测试三方编译:sdkconfig.h 在固件为 IDF 真件
// (否则 CONFIG_OBD_UI_RENDER_RES 判空,ui/JDM 各 TU 几何回退 360,
// 与经 app_main 显式取值的字体档位(240/466)分裂——真机错位根因);
// sim/tests 走 simulator/shims/include 的空替身,无 Kconfig。
#include "sdkconfig.h"

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

/** 渲染像素 → 母版像素(UIS 的逆;用于字体度量这类按渲染分辨率
 *  产出的值——如 lv_font 的 line_height/字形度量,回母版坐标换算)。 */
#define UIM(px) (((int32_t)(px) >= 0) \
                 ? (((int32_t)(px) * UI_MASTER_RES + (UI_RENDER_RES / 2)) / UI_RENDER_RES) \
                 : (((int32_t)(px) * UI_MASTER_RES - (UI_RENDER_RES / 2)) / UI_RENDER_RES))
