#pragma once
// ================================================================
//  sdkconfig.h — 模拟器/主机测试的 Kconfig 替身(空)
//
//  ui/JDM 经 core/jdm_geo.h 读 CONFIG_OBD_UI_RENDER_RES 折算几何;
//  主机端无 Kconfig,此文件留空 —— jdm_geo.h 自回退 360,sim 另经
//  -DUI_RENDER_RES=sim_ui_render_res() 跟随 --ui-res 运行时覆盖。
//  固件侧用 IDF 生成的真件,不经这里。
// ================================================================
