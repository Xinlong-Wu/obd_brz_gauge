#pragma once
// ================================================================
//  board_api.h — 板级抽象统一接口(M2.4,移植自 Hokori23/obd_brz_gauge)
//
//  一块编译目标一块板(Kconfig OBD_BOARD 选择,board_dispatch.c 静态分发):
//    WS185  微雪 ESP32-S3-Touch-LCD-1.85(360×360 IPS,ST77916 QSPI +
//           CST816 触摸 + TCA9554 IO 扩展,V1/V2/V3 由 OBD_HW_VERSION 细分)
//    WS175  微雪 ESP32-S3-Touch-AMOLED-1.75(466×466 AMOLED,CO5300 QSPI +
//           CST9217 触摸,共享 I2C 总线可挂 QMI8658 IMU / ADS1115)
//  app_main 只面向本 API;屏驱动/背光/触摸映射差异全部封在 boards/ 里。
// ================================================================

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

/** 面板刷屏(DMA)完成回调,签名同 esp_lcd_panel_io 事件回调。 */
typedef bool (*board_display_flush_ready_cb_t)(esp_lcd_panel_io_handle_t panel_io,
                                               esp_lcd_panel_io_event_data_t *edata,
                                               void *user_ctx);

/** 统一显示上下文:app_main 用它配置 LVGL display 与触摸。 */
typedef struct {
    uint16_t hor_res;
    uint16_t ver_res;
    uint16_t draw_buffer_lines;
    uint8_t color_bits;
    bool has_touch;
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_touch_handle_t touch;
} board_display_context_t;

/** 板卡静态描述(编译期常量)。 */
typedef struct {
    const char *name;
    uint16_t hor_res;
    uint16_t ver_res;
    uint16_t draw_buffer_lines;
    uint8_t color_bits;
    bool has_touch;
    bool touch_swap_xy;
    bool touch_mirror_x;
    bool touch_mirror_y;
} board_profile_t;

esp_err_t board_init(void);
esp_err_t board_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx);
esp_err_t board_display_init(board_display_context_t *ctx);
esp_err_t board_set_brightness(uint8_t percent);
esp_err_t board_get_shared_i2c_bus(i2c_master_bus_handle_t *out_bus);  // WS185 无共享总线 → NOT_SUPPORTED
esp_err_t board_i2c_reg_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t *data, size_t len);
esp_err_t board_i2c_reg_read(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len);
const board_profile_t *board_profile(void);
const char *board_name(void);
bool board_has_touch(void);
