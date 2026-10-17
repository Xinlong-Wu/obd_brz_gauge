#pragma once
// ================================================================
//  pages.h — JDM 页面注册表(原型 PAGES 数组顺序)
// ================================================================

#include "core/jdm_nav.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const jdm_page_t page_gear;       // 挡位 / 转速
extern const jdm_page_t page_boost;      // 增压 / 真空
extern const jdm_page_t page_coolant;    // 冷却液温度
extern const jdm_page_t page_oiltemp;    // 发动机油温
extern const jdm_page_t page_oilpress;   // 发动机油压
extern const jdm_page_t page_triple;     // 三项综合
extern const jdm_page_t page_multi;      // 多项综合
extern const jdm_page_t page_tyres;      // 四轮胎压
extern const jdm_page_t page_speed;      // 车速 / 路景
extern const jdm_page_t page_gforce;     // G 力
extern const jdm_page_t page_heart;      // 心率(源未接入,断连态)
extern const jdm_page_t page_analog;     // 机械压力表
extern const jdm_page_t page_music;      // 音乐(源未接入,断连态)
extern const jdm_page_t page_display;    // 日 / 夜亮度
extern const jdm_page_t page_bluetooth;  // 蓝牙 / 遥控
extern const jdm_page_t page_pair;       // 手机配对
extern const jdm_page_t page_obd;        // OBD 设备(源未接入,演示流程)

/** 轮播页表(jdm_ui 装配)。 */
extern const jdm_page_t *const jdm_pages[];
extern const uint8_t jdm_page_count;

#ifdef __cplusplus
}
#endif
