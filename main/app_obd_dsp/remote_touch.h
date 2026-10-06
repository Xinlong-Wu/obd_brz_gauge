#pragma once
// ================================================================
//  remote_touch.h — WiFi 远程触摸注入(虚拟指针输入设备)
//
//  Kconfig OBD_REMOTE_TOUCH 开启(依赖 OBD_SCREENSHOT——控制页是传输载体)。
//  取图控制页把手机上的按下/移动/抬起以 0–10000 归一化坐标 POST 到
//  /touch,本模块入队并由虚拟 indev 的 read_cb 逐个回放——LVGL 在同一条
//  路径上原生合成点击/滑动(LV_EVENT_GESTURE)/长按,UI 代码零改动。
//  与真实触摸面板并存(LVGL 支持多指针设备,各自独立出事件)。
// ================================================================
#include <stdbool.h>

#include "esp_err.h"
#include "lvgl.h"

/** 注册虚拟指针 indev(幂等;队列分配失败返回错误)。 */
esp_err_t remote_touch_register(lv_display_t *disp);

/** 虚拟 indev 是否已注册(/touch 端点据此决定 204 或 503)。 */
bool remote_touch_ready(void);

/** 注入一个触摸采样(归一化 0–10000;httpd 任务调用,非阻塞)。 */
void remote_touch_feed(int x_norm, int y_norm, bool pressed);
