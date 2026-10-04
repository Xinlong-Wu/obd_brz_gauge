#pragma once
// ================================================================
//  board_ws_128_scale.h — WS128 显示降采样输出层
//
//  UI 按 360×360 虚拟分辨率渲染(布局/主题/开机动画零改动),本模块在
//  LVGL flush 阶段把脏区并入全帧影子缓冲,3:2 最近邻降采样后分块 DMA
//  发往 240×240 GC9A01。面板 IO 的 on_color_trans_done 统一经
//  ws128_scale_on_color_trans_done 转发:归还分块信号量,最后一个
//  分块完成时再通知 LVGL flush_ready。
// ================================================================
#include "sdkconfig.h"

#if CONFIG_OBD_BOARD_WS_128_GC9A01

#include <stdbool.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include "bsp_obd_dsp/boards/board_api.h"

/** 面板 IO 传输完成回调(由 board_ws_128_gc9a01.c 挂到 panel IO 上)。 */
bool ws128_scale_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_io_event_data_t *edata,
                                     void *user_ctx);

/** 登记需要转发的 LVGL flush 完成回调(板级 register_display_flush_ready_callback 调用)。 */
void ws128_scale_set_flush_ready_chain(board_display_flush_ready_cb_t cb, void *user_ctx);

/** 分配影子/分块缓冲与信号量(panel 初始化后、LVGL 任务启动前调用一次)。 */
esp_err_t ws128_scale_init(esp_lcd_panel_handle_t panel);

/** LVGL flush 回调:影子合并 + 降采样 + 分块发屏。 */
void ws128_scale_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

#endif /* CONFIG_OBD_BOARD_WS_128_GC9A01 */
