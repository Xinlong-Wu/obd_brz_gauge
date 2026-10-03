// WS175 AMOLED 板级实现(CO5300 QSPI 面板 + CST9217 触摸 + 共享 I2C 总线)。
// 移植自 Hokori23/obd_brz_gauge(实车/实机验证过);与本仓库的差异:
//   - 旋转固定 180°(spec 头注释),ref 的 NVS 运行时旋转暂未引入
//   - 触摸初始化失败走本仓库 M2.3 的 nvs_error_log_recordf 记录
#include <string.h>
#include <inttypes.h>

#include "sdkconfig.h"

#if CONFIG_OBD_BOARD_WS_175_AMOLED

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst9217.h"

#include "bsp_obd_dsp/boards/board_api.h"
#include "bsp_obd_dsp/boards/board_ws_175_amoled_spec.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "esp_log.h"

static const char *TAG = "board_ws_175";

#define BOARD_WS_175_AMOLED_I2C_PORT          0
#define BOARD_WS_175_AMOLED_I2C_SCL           GPIO_NUM_14
#define BOARD_WS_175_AMOLED_I2C_SDA           GPIO_NUM_15
#define BOARD_WS_175_AMOLED_I2C_SPEED_HZ      400000

#define BOARD_WS_175_AMOLED_SPI_HOST          SPI2_HOST
#define BOARD_WS_175_AMOLED_LCD_CS            GPIO_NUM_12
#define BOARD_WS_175_AMOLED_LCD_PCLK          GPIO_NUM_38
#define BOARD_WS_175_AMOLED_LCD_DATA0         GPIO_NUM_4
#define BOARD_WS_175_AMOLED_LCD_DATA1         GPIO_NUM_5
#define BOARD_WS_175_AMOLED_LCD_DATA2         GPIO_NUM_6
#define BOARD_WS_175_AMOLED_LCD_DATA3         GPIO_NUM_7
#define BOARD_WS_175_AMOLED_LCD_RST           GPIO_NUM_39
#define BOARD_WS_175_AMOLED_TOUCH_RST         GPIO_NUM_40
#define BOARD_WS_175_AMOLED_TOUCH_INT         GPIO_NUM_11
#define BOARD_WS_175_AMOLED_LCD_TRANS_QUEUE   10

static i2c_master_bus_handle_t s_i2c_handle;
static esp_lcd_panel_handle_t s_panel_handle;
static esp_lcd_panel_io_handle_t s_panel_io_handle;
static esp_lcd_touch_handle_t s_touch_handle;
static board_display_flush_ready_cb_t s_flush_ready_cb;
static void *s_flush_ready_user_ctx;
static bool s_i2c_ready;
static bool s_spi_ready;

static board_profile_t s_board_profile = {
    .name = BOARD_WS_175_AMOLED_NAME,
    .hor_res = BOARD_WS_175_AMOLED_H_RES,
    .ver_res = BOARD_WS_175_AMOLED_V_RES,
    .draw_buffer_lines = BOARD_WS_175_AMOLED_DRAW_BUFFER_LINES,
    .color_bits = BOARD_WS_175_AMOLED_COLOR_BITS,
    .has_touch = BOARD_WS_175_AMOLED_HAS_TOUCH,
    .touch_swap_xy = BOARD_WS_175_AMOLED_TOUCH_SWAP_XY,
    .touch_mirror_x = BOARD_WS_175_AMOLED_TOUCH_MIRROR_X,
    .touch_mirror_y = BOARD_WS_175_AMOLED_TOUCH_MIRROR_Y,
};

static const co5300_lcd_init_cmd_t s_lcd_init_cmds[] = {
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    // LVGL 首帧渲染前保持黑屏,避免面板过早点亮闪出未初始化内容
    {0x51, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_CASET_X0 >> 8),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_CASET_X0 & 0xFF),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_CASET_X1 >> 8),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_CASET_X1 & 0xFF),
    }, 4, 0},
    {0x2B, (uint8_t[]){
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_RASET_Y0 >> 8),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_RASET_Y0 & 0xFF),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_RASET_Y1 >> 8),
        (uint8_t)(BOARD_WS_175_AMOLED_LCD_RASET_Y1 & 0xFF),
    }, 4, 600},
    {0x11, NULL, 0, 600},
    {0x29, NULL, 0, 0},
};

/** 在面板 IO 建好后补注册刷屏完成回调。 */
static esp_err_t board_ws_175_amoled_register_panel_io_callbacks(void)
{
    if (s_panel_io_handle == NULL || s_flush_ready_cb == NULL) {
        return ESP_OK;
    }

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = s_flush_ready_cb,
    };

    return esp_lcd_panel_io_register_event_callbacks(s_panel_io_handle, &cbs, s_flush_ready_user_ctx);
}

/** 初始化 WS175 共用 I2C 主总线(SDA15/SCL14,触摸与外挂传感器共用)。 */
static esp_err_t board_ws_175_amoled_i2c_init(void)
{
    if (s_i2c_ready) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BOARD_WS_175_AMOLED_I2C_SDA,
        .scl_io_num = BOARD_WS_175_AMOLED_I2C_SCL,
        .i2c_port = BOARD_WS_175_AMOLED_I2C_PORT,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_conf, &s_i2c_handle), TAG, "i2c init failed");
    s_i2c_ready = true;
    return ESP_OK;
}

static esp_err_t board_ws_175_amoled_open_i2c_device(uint8_t device_addr,
                                                     i2c_master_dev_handle_t *out_dev)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr,
        .scl_speed_hz = BOARD_WS_175_AMOLED_I2C_SPEED_HZ,
    };

    ESP_RETURN_ON_FALSE(out_dev != NULL, ESP_ERR_INVALID_ARG, TAG, "i2c device handle output is null");
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_i2c_init(), TAG, "i2c prepare failed");
    return i2c_master_bus_add_device(s_i2c_handle, &dev_cfg, out_dev);
}

/** 初始化 AMOLED 面板链路:QSPI 总线 → panel IO → 上电与寄存器初始化。 */
static esp_err_t board_ws_175_amoled_panel_init(void)
{
    if (s_panel_handle != NULL && s_panel_io_handle != NULL) {
        return ESP_OK;
    }

    if (!s_spi_ready) {
        const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(
            BOARD_WS_175_AMOLED_LCD_PCLK,
            BOARD_WS_175_AMOLED_LCD_DATA0,
            BOARD_WS_175_AMOLED_LCD_DATA1,
            BOARD_WS_175_AMOLED_LCD_DATA2,
            BOARD_WS_175_AMOLED_LCD_DATA3,
            BOARD_WS_175_AMOLED_TRANSFER_BYTES(BOARD_WS_175_AMOLED_DRAW_BUFFER_LINES));
        ESP_LOGI(TAG, "panel_init: spi_bus_initialize host=%d pclk=%d d0=%d d1=%d d2=%d d3=%d",
                 (int)BOARD_WS_175_AMOLED_SPI_HOST,
                 (int)BOARD_WS_175_AMOLED_LCD_PCLK,
                 (int)BOARD_WS_175_AMOLED_LCD_DATA0,
                 (int)BOARD_WS_175_AMOLED_LCD_DATA1,
                 (int)BOARD_WS_175_AMOLED_LCD_DATA2,
                 (int)BOARD_WS_175_AMOLED_LCD_DATA3);
        ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_WS_175_AMOLED_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO),
                            TAG, "spi init failed");
        s_spi_ready = true;
    }

    esp_lcd_panel_io_spi_config_t io_config =
        CO5300_PANEL_IO_QSPI_CONFIG(BOARD_WS_175_AMOLED_LCD_CS, s_flush_ready_cb, s_flush_ready_user_ctx);
    io_config.trans_queue_depth = BOARD_WS_175_AMOLED_LCD_TRANS_QUEUE;

    co5300_vendor_config_t vendor_config = {
        .init_cmds = s_lcd_init_cmds,
        .init_cmds_size = sizeof(s_lcd_init_cmds) / sizeof(s_lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_WS_175_AMOLED_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BOARD_WS_175_AMOLED_COLOR_BITS,
        .vendor_config = &vendor_config,
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_WS_175_AMOLED_SPI_HOST, &io_config,
                                 &s_panel_io_handle),
        TAG, "panel io init failed");
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_register_panel_io_callbacks(),
                        TAG, "panel io callback registration failed");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_co5300(s_panel_io_handle, &panel_config, &s_panel_handle),
                        TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel_handle,
                                              BOARD_WS_175_AMOLED_LCD_GAP_X,
                                              BOARD_WS_175_AMOLED_LCD_GAP_Y),
                        TAG, "panel gap setup failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel_handle), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel_handle), TAG, "panel controller init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel_handle, true), TAG, "panel enable failed");
    ESP_LOGI(TAG, "panel ready (rotation=%u via LVGL sw_rotate)",
             (unsigned)BOARD_WS_175_AMOLED_DISPLAY_ROTATION);
    return ESP_OK;
}

/** 初始化触摸控制器(CST9217,失败降级为无触摸继续跑)。 */
static esp_err_t board_ws_175_amoled_touch_init(void)
{
    if (s_touch_handle != NULL) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(board_ws_175_amoled_i2c_init(), TAG, "i2c prepare failed");

    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    touch_io_config.scl_speed_hz = BOARD_WS_175_AMOLED_I2C_SPEED_HZ;

    const esp_lcd_touch_config_t touch_config = {
        .x_max = BOARD_WS_175_AMOLED_H_RES,
        .y_max = BOARD_WS_175_AMOLED_V_RES,
        .rst_gpio_num = BOARD_WS_175_AMOLED_TOUCH_RST,
        .int_gpio_num = BOARD_WS_175_AMOLED_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = BOARD_WS_175_AMOLED_TOUCH_MIRROR_X,
            .mirror_y = BOARD_WS_175_AMOLED_TOUCH_MIRROR_Y,
        },
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_handle, &touch_io_config, &touch_io_handle),
                        TAG, "touch io init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst9217(touch_io_handle, &touch_config, &s_touch_handle),
                        TAG, "touch init failed");
    return ESP_OK;
}

/** 执行 WS175 板级基础初始化(面板/触摸延迟到 display_init)。 */
esp_err_t board_ws_175_amoled_init(void)
{
    return ESP_OK;
}

/** 注册 WS175 面板刷屏完成回调。 */
esp_err_t board_ws_175_amoled_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx)
{
    s_flush_ready_cb = cb;
    s_flush_ready_user_ctx = user_ctx;
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_register_panel_io_callbacks(),
                        TAG, "panel io callback registration failed");
    return ESP_OK;
}

/** 初始化 WS175 显示上下文:面板必成、触摸可降级。 */
esp_err_t board_ws_175_amoled_display_init(board_display_context_t *ctx)
{
    esp_err_t touch_err;

    ESP_RETURN_ON_FALSE(ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "display context is null");

    memset(ctx, 0, sizeof(*ctx));
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_panel_init(), TAG, "display init failed");
    touch_err = board_ws_175_amoled_touch_init();
    if (touch_err != ESP_OK) {
        ESP_LOGW(TAG, "touch init failed, continuing without touch: %s", esp_err_to_name(touch_err));
        nvs_error_log_recordf(TAG, touch_err, "touch init failed");
        s_touch_handle = NULL;
    }

    ctx->hor_res = BOARD_WS_175_AMOLED_H_RES;
    ctx->ver_res = BOARD_WS_175_AMOLED_V_RES;
    ctx->draw_buffer_lines = BOARD_WS_175_AMOLED_DRAW_BUFFER_LINES;
    ctx->color_bits = BOARD_WS_175_AMOLED_COLOR_BITS;
    ctx->has_touch = (s_board_profile.has_touch && touch_err == ESP_OK);
    ctx->panel = s_panel_handle;
    ctx->panel_io = s_panel_io_handle;
    ctx->touch = s_touch_handle;
    return ESP_OK;
}

/** 设置 WS175 面板亮度百分比(CMD 0x51,百分比换算 0-255)。 */
esp_err_t board_ws_175_amoled_set_brightness(uint8_t percent)
{
    uint8_t param;
    uint32_t lcd_cmd = BOARD_WS_175_AMOLED_BRIGHTNESS_CMD;

    ESP_RETURN_ON_FALSE(s_panel_handle != NULL && s_panel_io_handle != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "display is not initialized");
    ESP_RETURN_ON_FALSE(percent <= 100, ESP_ERR_INVALID_ARG, TAG, "brightness must be 0-100");

    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= 0x02 << 24;
    param = BOARD_WS_175_AMOLED_BRIGHTNESS_PARAM(percent);

    return esp_lcd_panel_io_tx_param(s_panel_io_handle, lcd_cmd, &param, 1);
}

/** 返回 WS175 板卡静态配置。 */
const board_profile_t *board_ws_175_amoled_profile(void)
{
    return &s_board_profile;
}

/** 返回 WS175 板卡名称。 */
const char *board_ws_175_amoled_name(void)
{
    return s_board_profile.name;
}

/** 返回 WS175 当前是否可用触摸。 */
bool board_ws_175_amoled_has_touch(void)
{
    return s_board_profile.has_touch;
}

esp_err_t board_ws_175_amoled_i2c_reg_write(uint8_t device_addr,
                                            uint8_t reg_addr,
                                            const uint8_t *data,
                                            size_t len)
{
    esp_err_t err;
    i2c_master_dev_handle_t dev = NULL;
    uint8_t payload[33];

    ESP_RETURN_ON_FALSE(len <= (sizeof(payload) - 1u), ESP_ERR_INVALID_ARG, TAG, "i2c write payload too large");
    ESP_RETURN_ON_FALSE((len == 0u) || (data != NULL), ESP_ERR_INVALID_ARG, TAG, "i2c write data is null");
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_open_i2c_device(device_addr, &dev), TAG, "open i2c device failed");

    payload[0] = reg_addr;
    if (len > 0u) {
        memcpy(&payload[1], data, len);
    }

    err = i2c_master_transmit(dev, payload, len + 1u, -1);
    i2c_master_bus_rm_device(dev);
    return err;
}

esp_err_t board_ws_175_amoled_i2c_reg_read(uint8_t device_addr,
                                           uint8_t reg_addr,
                                           uint8_t *data,
                                           size_t len)
{
    esp_err_t err;
    i2c_master_dev_handle_t dev = NULL;

    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "i2c read data is null");
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_open_i2c_device(device_addr, &dev), TAG, "open i2c device failed");
    err = i2c_master_transmit_receive(dev, &reg_addr, 1, data, len, -1);
    i2c_master_bus_rm_device(dev);
    return err;
}

/** 导出 WS175 共享 I2C 总线句柄(QMI8658 / ADS1115 挂这里)。 */
esp_err_t board_ws_175_amoled_get_shared_i2c_bus(i2c_master_bus_handle_t *out_bus)
{
    ESP_RETURN_ON_FALSE(out_bus != NULL, ESP_ERR_INVALID_ARG, TAG, "out_bus is null");
    ESP_RETURN_ON_ERROR(board_ws_175_amoled_i2c_init(), TAG, "i2c prepare failed");
    *out_bus = s_i2c_handle;
    return ESP_OK;
}

#endif /* CONFIG_OBD_BOARD_WS_175_AMOLED */
