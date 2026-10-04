// WS128 板级实现(GC9A01 四线 SPI 面板 + LEDC 背光,无触摸/无共享 I2C)。
// 板上 I2C(6/7)只挂本仓库不使用的 QMI8658,保持不初始化;
// UI 虚拟分辨率渲染与降采样输出见 board_ws_128_scale.c。
#include <string.h>
#include <inttypes.h>

#include "sdkconfig.h"

#if CONFIG_OBD_BOARD_WS_128_GC9A01

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#include "bsp_obd_dsp/boards/board_api.h"
#include "bsp_obd_dsp/boards/board_ws_128_gc9a01_spec.h"
#include "bsp_obd_dsp/boards/ui_scale.h"
#include "esp_log.h"

static const char *TAG = "board_ws_128";

static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_panel_io_handle;
static bool s_backlight_ready;

static board_profile_t s_board_profile = {
    .name = BOARD_WS_128_GC9A01_NAME,
    .hor_res = BOARD_WS_128_GC9A01_H_RES,
    .ver_res = BOARD_WS_128_GC9A01_V_RES,
    .draw_buffer_lines = BOARD_WS_128_GC9A01_DRAW_BUFFER_LINES,
    .color_bits = BOARD_WS_128_GC9A01_COLOR_BITS,
    .has_touch = BOARD_WS_128_GC9A01_HAS_TOUCH,
    .touch_swap_xy = 0,
    .touch_mirror_x = 0,
    .touch_mirror_y = 0,
};

/** LEDC PWM 背光(高电平点亮)。 */
static esp_err_t board_ws_128_backlight_init(void)
{
    if (s_backlight_ready) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer = {
        .duty_resolution = BOARD_WS_128_GC9A01_BL_RESOLUTION,
        .freq_hz = BOARD_WS_128_GC9A01_BL_FREQ_HZ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer init failed");

    const ledc_channel_config_t channel = {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,   // 先灭屏,面板初始化完成后再恢复默认亮度
        .gpio_num = BOARD_WS_128_GC9A01_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel init failed");
    s_backlight_ready = true;
    return ESP_OK;
}

/** DMA 传输完成的直通回调:直接转发 LVGL flush_ready(native 渲染路径,
 *  ui_scale 未激活时使用——缩放回调会触碰未初始化的信号量,不能无条件挂)。 */
static bool native_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

/** 当前挂接的传输完成回调(native=直通 / ui_scale=缩放层),供延迟重挂。 */
static board_display_flush_ready_cb_t s_flush_ready_cb;
static void *s_flush_ready_user_ctx;

/** 按当前模式把正确的完成回调挂到 panel IO 上。 */
static esp_err_t board_ws_128_wire_trans_done(void)
{
    if (s_panel_io_handle == NULL) {
        return ESP_OK;
    }
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = ui_scale_active()
                                   ? ui_scale_on_color_trans_done
                                   : native_color_trans_done,
    };
    return esp_lcd_panel_io_register_event_callbacks(s_panel_io_handle, &cbs,
                                                     ui_scale_active()
                                                         ? NULL
                                                         : s_flush_ready_user_ctx);
}

/** 初始化 GC9A01 面板链路:四线 SPI 总线 → panel IO → 上电复位。 */
static esp_err_t board_ws_128_panel_init(void)
{
    if (s_panel_handle != NULL && s_panel_io_handle != NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(board_ws_128_backlight_init(), TAG, "backlight prepare failed");

    const spi_bus_config_t buscfg = {
        .mosi_io_num = BOARD_WS_128_GC9A01_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_WS_128_GC9A01_LCD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_WS_128_GC9A01_H_RES * BOARD_WS_128_GC9A01_DRAW_BUFFER_LINES * 2,
    };
    ESP_LOGI(TAG, "panel_init: spi_bus_initialize host=%d mosi=%d clk=%d",
             (int)BOARD_WS_128_GC9A01_SPI_HOST,
             (int)BOARD_WS_128_GC9A01_LCD_MOSI,
             (int)BOARD_WS_128_GC9A01_LCD_CLK);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_WS_128_GC9A01_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                        TAG, "spi init failed");

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BOARD_WS_128_GC9A01_LCD_DC,
        .cs_gpio_num = BOARD_WS_128_GC9A01_LCD_CS,
        .pclk_hz = BOARD_WS_128_GC9A01_LCD_SCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = BOARD_WS_128_GC9A01_LCD_TRANS_QUEUE,
        .on_color_trans_done = native_color_trans_done,   // 默认直通;ui_scale 激活时重挂
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_WS_128_GC9A01_SPI_HOST,
                                 &io_config, &s_panel_io_handle),
        TAG, "panel io init failed");

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_WS_128_GC9A01_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BOARD_WS_128_GC9A01_COLOR_BITS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_gc9a01(s_panel_io_handle, &panel_config, &s_panel_handle),
                        TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "panel controller init failed");
#if BOARD_WS_128_GC9A01_MIRROR_X || BOARD_WS_128_GC9A01_MIRROR_Y
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel_handle,
                                             BOARD_WS_128_GC9A01_MIRROR_X,
                                             BOARD_WS_128_GC9A01_MIRROR_Y),
                        TAG, "panel mirror failed");
#endif
#if BOARD_WS_128_GC9A01_INVERT_COLOR
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel_handle, true),
                        TAG, "panel invert failed");
#endif
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true), TAG, "panel enable failed");
    ESP_LOGI(TAG, "panel ready (physical %ux%u; render res via CONFIG_OBD_UI_RENDER_RES)",
             (unsigned)BOARD_WS_128_GC9A01_H_RES, (unsigned)BOARD_WS_128_GC9A01_V_RES);
    return ESP_OK;
}

/** 执行 WS128 板级基础初始化(面板/背光延迟到 display_init)。 */
esp_err_t board_ws_128_init(void)
{
    return ESP_OK;
}

/** 注册 WS128 面板刷屏完成回调(按当前模式直通或经缩放层转发)。 */
esp_err_t board_ws_128_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx)
{
    s_flush_ready_cb = cb;
    s_flush_ready_user_ctx = user_ctx;
    if (!ui_scale_active()) {
        ui_scale_set_flush_ready_chain(cb, user_ctx);   // native: 直通回调要用
    }
    return board_ws_128_wire_trans_done();
}

/** app_main 在 display_init 之后决定是否启用 ui_scale;此处按最终模式重挂回调。 */
esp_err_t board_ws_128_notify_output_mode(bool scaled_output)
{
    if (scaled_output) {
        ui_scale_set_flush_ready_chain(s_flush_ready_cb, s_flush_ready_user_ctx);
    }
    return board_ws_128_wire_trans_done();
}

/** 初始化 WS128 显示上下文(无触摸,纯显示)。 */
esp_err_t board_ws_128_display_init(board_display_context_t *ctx)
{
    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "display context is null");

    memset(ctx, 0, sizeof(*ctx));
    ESP_RETURN_ON_ERROR(board_ws_128_panel_init(), TAG, "display init failed");

    ctx->hor_res = BOARD_WS_128_GC9A01_H_RES;
    ctx->ver_res = BOARD_WS_128_GC9A01_V_RES;
    ctx->draw_buffer_lines = BOARD_WS_128_GC9A01_DRAW_BUFFER_LINES;
    ctx->color_bits = BOARD_WS_128_GC9A01_COLOR_BITS;
    ctx->has_touch = false;
    ctx->panel = s_panel_handle;
    ctx->panel_io = s_panel_io_handle;
    ctx->touch = NULL;
    return ESP_OK;
}

/** 设置背光亮度百分比(LEDC 占空比,高电平点亮)。 */
esp_err_t board_ws_128_set_brightness(uint8_t percent)
{
    ESP_RETURN_ON_FALSE(s_backlight_ready, ESP_ERR_INVALID_STATE, TAG, "backlight is not initialized");
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "brightness must be 0-100");

    const uint32_t duty =
        ((uint32_t)percent * BOARD_WS_128_GC9A01_BL_MAX_DUTY) / 100u;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty),
                        TAG, "backlight duty set failed");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/** 返回 WS128 板卡静态配置。 */
const board_profile_t *board_ws_128_profile(void)
{
    return &s_board_profile;
}

/** 返回 WS128 板卡名称。 */
const char *board_ws_128_name(void)
{
    return s_board_profile.name;
}

/** 返回 WS128 当前是否可用触摸(恒否)。 */
bool board_ws_128_has_touch(void)
{
    return s_board_profile.has_touch;
}

#endif /* CONFIG_OBD_BOARD_WS_128_GC9A01 */
