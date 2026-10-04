// board_dispatch.c — 按 Kconfig 编译目标静态分发到对应板级实现。
#include "bsp_obd_dsp/boards/board_api.h"

esp_err_t board_ws_185_init(void);
esp_err_t board_ws_185_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx);
esp_err_t board_ws_185_display_init(board_display_context_t *ctx);
esp_err_t board_ws_185_set_brightness(uint8_t percent);
esp_err_t board_ws_185_i2c_reg_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t *data, size_t len);
esp_err_t board_ws_185_i2c_reg_read(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len);
const board_profile_t *board_ws_185_profile(void);
const char *board_ws_185_name(void);
bool board_ws_185_has_touch(void);

esp_err_t board_ws_175_amoled_init(void);
esp_err_t board_ws_175_amoled_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx);
esp_err_t board_ws_175_amoled_display_init(board_display_context_t *ctx);
esp_err_t board_ws_175_amoled_set_brightness(uint8_t percent);
esp_err_t board_ws_175_amoled_i2c_reg_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t *data, size_t len);
esp_err_t board_ws_175_amoled_i2c_reg_read(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len);
const board_profile_t *board_ws_175_amoled_profile(void);
const char *board_ws_175_amoled_name(void);
bool board_ws_175_amoled_has_touch(void);

esp_err_t board_ws_128_init(void);
esp_err_t board_ws_128_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx);
esp_err_t board_ws_128_display_init(board_display_context_t *ctx);
esp_err_t board_ws_128_set_brightness(uint8_t percent);
esp_err_t board_ws_128_notify_output_mode(bool scaled_output);
const board_profile_t *board_ws_128_profile(void);
const char *board_ws_128_name(void);
bool board_ws_128_has_touch(void);

esp_err_t board_init(void)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_init();
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_init();
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_init();
#else
#error "No board selected"
#endif
}

esp_err_t board_register_display_flush_ready_callback(board_display_flush_ready_cb_t cb, void *user_ctx)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_register_display_flush_ready_callback(cb, user_ctx);
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_register_display_flush_ready_callback(cb, user_ctx);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_register_display_flush_ready_callback(cb, user_ctx);
#else
#error "No board selected"
#endif
}

esp_err_t board_notify_output_mode(bool scaled_output)
{
#if CONFIG_OBD_BOARD_WS_185
    (void)scaled_output;
    return ESP_OK;   // 面板完成回调经 ST77916 静态接线,模式无关
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    (void)scaled_output;
    return ESP_OK;   // 同上:panel IO 回调为静态直通(espressif 组件管理)
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_notify_output_mode(scaled_output);
#else
#error "No board selected"
#endif
}

esp_err_t board_display_init(board_display_context_t *ctx)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_display_init(ctx);
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_display_init(ctx);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_display_init(ctx);
#else
#error "No board selected"
#endif
}

esp_err_t board_set_brightness(uint8_t percent)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_set_brightness(percent);
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_set_brightness(percent);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_set_brightness(percent);
#else
#error "No board selected"
#endif
}

esp_err_t board_get_shared_i2c_bus(i2c_master_bus_handle_t *out_bus)
{
#if CONFIG_OBD_BOARD_WS_185
    (void)out_bus;
    return ESP_ERR_NOT_SUPPORTED;   // WS185 的 I2C 由传统驱动自管
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    extern esp_err_t board_ws_175_amoled_get_shared_i2c_bus(i2c_master_bus_handle_t *out_bus);
    return board_ws_175_amoled_get_shared_i2c_bus(out_bus);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    (void)out_bus;
    return ESP_ERR_NOT_SUPPORTED;   // 板上 I2C 只挂本仓库不用的 QMI8658,不初始化
#else
#error "No board selected"
#endif
}

esp_err_t board_i2c_reg_write(uint8_t device_addr, uint8_t reg_addr, const uint8_t *data, size_t len)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_i2c_reg_write(device_addr, reg_addr, data, len);
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_i2c_reg_write(device_addr, reg_addr, data, len);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    (void)device_addr; (void)reg_addr; (void)data; (void)len;
    return ESP_ERR_NOT_SUPPORTED;   // 无共享 I2C 设备
#else
#error "No board selected"
#endif
}

esp_err_t board_i2c_reg_read(uint8_t device_addr, uint8_t reg_addr, uint8_t *data, size_t len)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_i2c_reg_read(device_addr, reg_addr, data, len);
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_i2c_reg_read(device_addr, reg_addr, data, len);
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    (void)device_addr; (void)reg_addr; (void)data; (void)len;
    return ESP_ERR_NOT_SUPPORTED;   // 无共享 I2C 设备
#else
#error "No board selected"
#endif
}

const board_profile_t *board_profile(void)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_profile();
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_profile();
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_profile();
#else
#error "No board selected"
#endif
}

const char *board_name(void)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_name();
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_name();
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_name();
#else
#error "No board selected"
#endif
}

bool board_has_touch(void)
{
#if CONFIG_OBD_BOARD_WS_185
    return board_ws_185_has_touch();
#elif CONFIG_OBD_BOARD_WS_175_AMOLED
    return board_ws_175_amoled_has_touch();
#elif CONFIG_OBD_BOARD_WS_128_GC9A01
    return board_ws_128_has_touch();
#else
#error "No board selected"
#endif
}
