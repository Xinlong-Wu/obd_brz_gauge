// screen_capture.c — WiFi 取图数据源(实现见同名头文件)。
// 影子缓冲随 flush 增量更新(总在 LVGL 锁内,读取侧拿 lvgl_mux 短持锁
// 快照即可得到一致帧);JPEG 输入走 16 字节对齐持久缓冲(esp_new_jpeg
// 要求),编码在锁外进行,编码器句柄由内部互斥锁串行化(httpd 与流共用)。
#include "sdkconfig.h"

#if CONFIG_OBD_SCREENSHOT

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_jpeg_enc.h"
#include "lvgl.h"

#include "app_obd_dsp/screen_capture.h"
#include "export_path/ui_res.h"

static const char *TAG = "screen_capture";

static uint16_t *s_shadow;          // UI_RENDER_RES² RGB565(大端,与 flush 字节序一致)
static uint16_t s_res;
static bool s_ready;

// JPEG 编码器(懒打开,httpd 快照与流任务共用,内部互斥串行化)
static SemaphoreHandle_t s_jpeg_mtx;
static jpeg_enc_handle_t s_jpeg;
static int64_t s_encoder_retry_after_us;   // 编码器打开失败后的退避截止时间
static uint8_t *s_jpeg_in;          // 16 字节对齐的整帧输入(jpeg_calloc_align)
static size_t s_jpeg_in_bytes;

esp_err_t screen_capture_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }
    s_res = (uint16_t)UI_RENDER_RES;
    const size_t bytes = (size_t)s_res * s_res * sizeof(uint16_t);
    s_shadow = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (s_shadow == NULL) {
        ESP_LOGW(TAG, "shadow alloc failed (%u B), WiFi capture disabled", (unsigned)bytes);
        return ESP_ERR_NO_MEM;
    }
    memset(s_shadow, 0, bytes);
    s_ready = true;
    ESP_LOGI(TAG, "capture shadow ready: %ux%u RGB565, %u B PSRAM",
             s_res, s_res, (unsigned)bytes);
    return ESP_OK;
}

bool screen_capture_ready(void)
{
    return s_ready;
}

uint16_t screen_capture_res(void)
{
    return s_res;
}

void screen_capture_on_flush(const lv_area_t *area, const uint8_t *px_map)
{
    if (!s_ready) {
        return;
    }
    const int w = area->x2 - area->x1 + 1;
    const uint16_t *color_map = (const uint16_t *)px_map;   // RGB565_SWAPPED,2 字节/像素
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(&s_shadow[(uint32_t)y * s_res + area->x1],
               &color_map[(uint32_t)(y - area->y1) * w],
               (uint32_t)w * sizeof(uint16_t));
    }
}

/** 短持 lvgl 锁把影子快照进对齐编码输入缓冲(flush 只在 LVGL 锁内发生)。
 *  影子是大端 RGB565,编码器只收 RGB888(见 esp_jpeg_common.h 格式表),
 *  拷贝时逐像素展开。 */
static esp_err_t screen_capture_snapshot_aligned(void)
{
    extern SemaphoreHandle_t lvgl_mux;   // app_main.c 全局(BLE 扫描页同款用法)
    if (lvgl_mux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    const size_t px = (size_t)s_res * s_res;
    const uint16_t *src = s_shadow;
    uint8_t *dst = s_jpeg_in;
    for (size_t i = 0; i < px; i++) {
        const uint16_t v = (uint16_t)((src[i] << 8) | (src[i] >> 8));   // unswap
        const uint16_t r5 = (v >> 11) & 0x1f, g6 = (v >> 5) & 0x3f, b5 = v & 0x1f;
        *dst++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        *dst++ = (uint8_t)((g6 << 2) | (g6 >> 4));
        *dst++ = (uint8_t)((b5 << 3) | (b5 >> 2));
    }
    xSemaphoreGive(lvgl_mux);
    return ESP_OK;
}

static esp_err_t screen_capture_ensure_encoder(void)
{
    if (s_jpeg != NULL) {
        return ESP_OK;
    }
    // 失败冷却:编码器打开失败(如组件不支持/内存不足)时退避 5s,
    // 避免流任务每帧重试刷屏 + 输入缓冲反复分配
    if (esp_timer_get_time() < s_encoder_retry_after_us) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_jpeg_in == NULL) {   // 只分配一次:失败重试不得重复分配(泄漏)
        s_jpeg_in_bytes = (size_t)s_res * s_res * 3;   // RGB888
        s_jpeg_in = jpeg_calloc_align(s_jpeg_in_bytes, 16);
        ESP_RETURN_ON_FALSE(s_jpeg_in != NULL, ESP_ERR_NO_MEM, TAG, "jpeg input alloc failed");
    }

    jpeg_enc_config_t cfg = DEFAULT_JPEG_ENC_CONFIG();
    cfg.width = s_res;
    cfg.height = s_res;
    cfg.src_type = JPEG_PIXEL_FORMAT_RGB888;   // 编码器不支持 RGB565_BE/LE(仅解码支持)
    cfg.subsampling = JPEG_SUBSAMPLE_420;
    cfg.quality = 50;
    cfg.task_enable = false;
    const jpeg_error_t err = jpeg_enc_open(&cfg, &s_jpeg);
    if (err != JPEG_ERR_OK) {
        s_encoder_retry_after_us = esp_timer_get_time() + 5000000;
        ESP_LOGE(TAG, "jpeg encoder open failed: %d (retry after 5s)", err);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "jpeg encoder open (res %u, RGB888, q%u)", s_res, cfg.quality);
    return ESP_OK;
}

esp_err_t screen_capture_jpeg(uint8_t *out, size_t cap, int *out_size)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_jpeg_mtx == NULL) {
        s_jpeg_mtx = xSemaphoreCreateMutex();
    }
    ESP_RETURN_ON_FALSE(s_jpeg_mtx != NULL, ESP_ERR_NO_MEM, TAG, "jpeg mutex alloc failed");

    // 顺序不可换:先 ensure(分配输入缓冲),再快照(往缓冲里写)
    ESP_RETURN_ON_ERROR(screen_capture_ensure_encoder(), TAG, "encoder init failed");
    ESP_RETURN_ON_ERROR(screen_capture_snapshot_aligned(), TAG, "snapshot failed");

    xSemaphoreTake(s_jpeg_mtx, portMAX_DELAY);
    esp_err_t err = jpeg_enc_process(s_jpeg, s_jpeg_in, (int)s_jpeg_in_bytes,
                                     out, (int)cap, out_size);
    xSemaphoreGive(s_jpeg_mtx);
    if (err != JPEG_ERR_OK) {
        ESP_LOGW(TAG, "jpeg encode failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

size_t screen_capture_bmp_size(void)
{
    const size_t row = ((size_t)s_res * 3 + 3) / 4 * 4;   // BMP 行 4 字节对齐(466 时需 pad)
    return 54 + row * s_res;
}

esp_err_t screen_capture_bmp(uint8_t *out, size_t cap, int *out_size)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    const size_t row = ((size_t)s_res * 3 + 3) / 4 * 4;
    const size_t total = 54 + row * s_res;
    ESP_RETURN_ON_FALSE(cap >= total, ESP_ERR_NO_MEM, TAG, "bmp buffer too small");

    extern SemaphoreHandle_t lvgl_mux;
    if (lvgl_mux == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // 文件头(14)+ DIB(40):24bpp、底行向上、无压缩
    const uint32_t size_image = (uint32_t)row * s_res;
    uint8_t *h = out;
    memset(h, 0, 54);
    h[0] = 'B'; h[1] = 'M';
    h[2] = (uint8_t)(total); h[3] = (uint8_t)(total >> 8);
    h[4] = (uint8_t)(total >> 16); h[5] = (uint8_t)(total >> 24);
    h[10] = 54;                                // bfOffBits = 54
    h[14] = 40;                                // biSize
    h[18] = (uint8_t)(s_res); h[19] = (uint8_t)(s_res >> 8);            // biWidth
    h[22] = (uint8_t)(s_res); h[23] = (uint8_t)(s_res >> 8);            // biHeight(正 → 底行向上)
    h[26] = 1;                                 // biPlanes
    h[28] = 24;                                // biBitCount
    h[34] = (uint8_t)(size_image); h[35] = (uint8_t)(size_image >> 8);
    h[36] = (uint8_t)(size_image >> 16); h[37] = (uint8_t)(size_image >> 24);

    // 像素:影子为 LV_COLOR_16_SWAP 大端 RGB565 → BGR,底行向上 + 行尾 pad
    for (int src_y = (int)s_res - 1, dst_y = 0; src_y >= 0; src_y--, dst_y++) {
        uint8_t *rowp = out + 54 + (size_t)dst_y * row;
        const uint16_t *src = &s_shadow[(uint32_t)src_y * s_res];
        for (int x = 0; x < s_res; x++) {
            uint16_t v = (uint16_t)((src[x] << 8) | (src[x] >> 8));   // unswap
            const uint16_t r5 = (v >> 11) & 0x1f, g6 = (v >> 5) & 0x3f, b5 = v & 0x1f;
            *rowp++ = (uint8_t)((b5 << 3) | (b5 >> 2));
            *rowp++ = (uint8_t)((g6 << 2) | (g6 >> 4));
            *rowp++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        }
        for (size_t p = (size_t)s_res * 3; p < row; p++) {
            rowp[p - (size_t)s_res * 3] = 0;
        }
    }
    xSemaphoreGive(lvgl_mux);
    *out_size = (int)total;
    return ESP_OK;
}

#endif /* CONFIG_OBD_SCREENSHOT */
