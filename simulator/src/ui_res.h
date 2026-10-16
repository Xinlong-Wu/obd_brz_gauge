#pragma once
// ================================================================
//  ui_res.h — UIS()/UIM() 的模拟器运行时分辨率
//
//  固件的 UI_RENDER_RES 是编译期 CONFIG 宏;sim 的 --ui-res 运行时才
//  确定。CMake 以 -DUI_RENDER_RES=sim_ui_render_res() 覆盖 jdm_geo.h
//  的编译期回退(其 #ifndef 守卫让 -D 优先),并以 -include 强制注入
//  本声明——否则 240/466 档下几何仍按 360 折算,布局与真机错位。
// ================================================================

int sim_ui_render_res(void);
