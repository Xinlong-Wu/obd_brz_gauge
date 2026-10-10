#pragma once
// ================================================================
//  screen_capture.h — WiFi 取图数据源(影子帧缓冲 + BMP/JPEG 编码)
//
//  LVGL 分块刷屏不保留整帧,本模块在 flush 路径把每次脏区并入一份
//  PSRAM 全帧影子(编译期开关 OBD_SCREENSHOT),供取图服务器输出:
//    screen_capture_bmp()   精确色 24-bit BMP(调试/像素对照)
//    screen_capture_jpeg()  JPEG 快照(esp_new_jpeg 软编码,页面下载/流共用)
//  写入侧(lvgl_flush_cb)总在 LVGL 锁内;读取侧用 lvgl_mux 短持锁快照,
//  编码在锁外(详见 .c)。
// ================================================================
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

/** 分配影子缓冲(板级初始化后调用一次;失败则功能禁用并告警)。 */
esp_err_t screen_capture_init(void);

/** 影子是否可用(Kconfig 关闭或分配失败时为 false)。 */
bool screen_capture_ready(void);

/** 影子分辨率(= UI_RENDER_RES)。 */
uint16_t screen_capture_res(void);

/** flush 钩子:并入脏区(lvgl_flush_cb 调用,须在 LVGL 锁内)。 */
void screen_capture_on_flush(const lv_area_t *area, const uint8_t *px_map);   // LVGL9:px_map 为 RGB565_SWAPPED 字节流,内存布局与 v8 LV_COLOR_16_SWAP 一致

/** 编码当前帧为 JPEG(out 需 ≥ res²×2 容量;内部短持 lvgl 锁快照)。 */
esp_err_t screen_capture_jpeg(uint8_t *out, size_t cap, int *out_size);

/** 当前帧转 24-bit BMP(out 需 ≥ screen_capture_bmp_size())。 */
esp_err_t screen_capture_bmp(uint8_t *out, size_t cap, int *out_size);

/** BMP 输出字节数(54 头 + 底行向上 + 行 4 字节对齐 padding)。 */
size_t screen_capture_bmp_size(void);
