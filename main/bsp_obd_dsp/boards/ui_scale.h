#pragma once
// ================================================================
//  ui_scale.h — 渲染分辨率 → 面板分辨率 的通用缩放输出层
//
//  布局按 720 母版经 UIS() 在编译期缩放到 CONFIG_OBD_UI_RENDER_RES
//  (export_path/ui_res.h),正常情况下渲染分辨率 = 面板分辨率,直通
//  lvgl_flush_cb。当两者不同(有意配置或调试)时本层接管:
//  LVGL flush 先并入全帧影子缓冲,再按方向分块发往面板——
//    下采样(S>D): 盒式滤波(覆盖源矩形逐分量平均)
//    上采样(S<D): 最近邻(备用路径,后续可升级双线性)
//  面板 IO 的 on_color_trans_done 统一经 ui_scale_on_color_trans_done:
//  归还分块信号量,最后一个分块完成时转发 LVGL flush_ready。
// ================================================================
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"

#include "bsp_obd_dsp/boards/board_api.h"

/** 面板 IO 传输完成回调(由启用缩放的板级实现挂到 panel IO 上)。 */
bool ui_scale_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                  esp_lcd_panel_io_event_data_t *edata,
                                  void *user_ctx);

/** 登记需要转发的 LVGL flush 完成回调(板级 register_display_flush_ready_callback 调用)。 */
void ui_scale_set_flush_ready_chain(board_display_flush_ready_cb_t cb, void *user_ctx);

/** 分配影子/分块缓冲与信号量(panel 初始化后、LVGL 任务启动前调用一次)。
 *  src_res/dst_res 为方形边长(渲染分辨率/面板分辨率,任意组合)。 */
esp_err_t ui_scale_init(esp_lcd_panel_handle_t panel, uint16_t src_res, uint16_t dst_res);

/** LVGL flush 回调:影子合并 + 按方向缩放 + 分块发屏。 */
void ui_scale_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

/** 当前是否已启用(诊断/日志用)。 */
bool ui_scale_active(void);
