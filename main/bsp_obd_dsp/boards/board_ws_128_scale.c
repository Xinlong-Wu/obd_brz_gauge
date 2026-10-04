// board_ws_128_scale.c — WS128 显示降采样输出层(实现见同名头文件)。
// 影子缓冲放 PSRAM(360×360×2 ≈ 259KB,2MB Quad PSRAM 可容纳),分块输出
// 缓冲放内部 DMA 内存,信号量保证复用前上次 DMA 已完成。
#include "sdkconfig.h"

#if CONFIG_OBD_BOARD_WS_128_GC9A01

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "bsp_obd_dsp/boards/board_api.h"
#include "bsp_obd_dsp/boards/board_ws_128_gc9a01_spec.h"
#include "bsp_obd_dsp/boards/board_ws_128_scale.h"

static const char *TAG = "board_ws_128";

#define WS128_SCALE_CHUNK_LINES 20
#define WS128_SCALE_CHUNK_BYTES \
    ((uint32_t)BOARD_WS_128_GC9A01_H_RES * WS128_SCALE_CHUNK_LINES * sizeof(uint16_t))

static uint16_t *s_shadow;                        // 虚拟 360×360 全帧影子(PSRAM)
static uint16_t *s_chunk;                         // 240×20 降采样输出块(内部 DMA 内存)
static SemaphoreHandle_t s_chunk_dma_done;        // 分块缓冲空闲信号量
static volatile int s_pending_chunks;             // 未完成的分块数,归零时转发 LVGL flush_ready
static esp_lcd_panel_handle_t s_panel;
static board_display_flush_ready_cb_t s_flush_ready_cb;
static void *s_flush_ready_user_ctx;

bool ws128_scale_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                     esp_lcd_panel_io_event_data_t *edata,
                                     void *user_ctx)
{
    (void)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(s_chunk_dma_done, &high_task_wakeup);
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    if (__atomic_sub_fetch(&s_pending_chunks, 1, __ATOMIC_RELAXED) == 0 && s_flush_ready_cb != NULL) {
        s_flush_ready_cb(panel_io, edata, s_flush_ready_user_ctx);
    }
    return false;
}

void ws128_scale_set_flush_ready_chain(board_display_flush_ready_cb_t cb, void *user_ctx)
{
    s_flush_ready_cb = cb;
    s_flush_ready_user_ctx = user_ctx;
}

esp_err_t ws128_scale_init(esp_lcd_panel_handle_t panel)
{
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_ARG, TAG, "panel handle is null");
    s_panel = panel;

    if (s_shadow == NULL) {
        s_shadow = heap_caps_malloc(
            (uint32_t)BOARD_WS_128_GC9A01_UI_RES * BOARD_WS_128_GC9A01_UI_RES * sizeof(uint16_t),
            MALLOC_CAP_SPIRAM);
    }
    if (s_chunk == NULL) {
        s_chunk = heap_caps_malloc(WS128_SCALE_CHUNK_BYTES, MALLOC_CAP_DMA);
    }
    if (s_chunk_dma_done == NULL) {
        s_chunk_dma_done = xSemaphoreCreateBinary();
        if (s_chunk_dma_done != NULL) {
            xSemaphoreGive(s_chunk_dma_done);   // 分块缓冲初始空闲
        }
    }

    if (s_shadow == NULL || s_chunk == NULL || s_chunk_dma_done == NULL) {
        ESP_LOGE(TAG, "scale buffers alloc failed: shadow=%p chunk=%p sem=%p",
                 s_shadow, s_chunk, (void *)s_chunk_dma_done);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "virtual %ux%u -> panel %ux%u downscale ready (shadow %u B PSRAM, chunk %u B DMA)",
             (unsigned)BOARD_WS_128_GC9A01_UI_RES, (unsigned)BOARD_WS_128_GC9A01_UI_RES,
             (unsigned)BOARD_WS_128_GC9A01_H_RES, (unsigned)BOARD_WS_128_GC9A01_V_RES,
             (unsigned)((uint32_t)BOARD_WS_128_GC9A01_UI_RES * BOARD_WS_128_GC9A01_UI_RES * sizeof(uint16_t)),
             (unsigned)WS128_SCALE_CHUNK_BYTES);
    return ESP_OK;
}

/** 3:2 最近邻:输出像素覆盖的源区间中心(360→240,倍率恒定 3/2)。 */
static inline uint16_t ws128_scale_src(uint16_t out)
{
    uint32_t s = ((uint32_t)out * 3u + 1u) / 2u;
    if (s >= BOARD_WS_128_GC9A01_UI_RES) {
        s = BOARD_WS_128_GC9A01_UI_RES - 1u;
    }
    return (uint16_t)s;
}

void ws128_scale_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    (void)drv;

    /* 1. LVGL 渲染缓冲行按脏区宽度紧排,先并入全帧影子 */
    const int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(&s_shadow[(uint32_t)y * BOARD_WS_128_GC9A01_UI_RES + area->x1],
               &color_map[(uint32_t)(y - area->y1) * w],
               (uint32_t)w * sizeof(uint16_t));
    }

    /* 2. 脏区映射到物理面板(2/3,上界向外取整保证覆盖) */
    const int panel_max = BOARD_WS_128_GC9A01_H_RES - 1;
    const int sx1 = (area->x1 * 2) / 3;
    const int sy1 = (area->y1 * 2) / 3;
    int sx2 = ((area->x2 + 1) * 2 + 2) / 3 - 1;
    int sy2 = ((area->y2 + 1) * 2 + 2) / 3 - 1;
    if (sx2 > panel_max) sx2 = panel_max;
    if (sy2 > panel_max) sy2 = panel_max;

    /* 3. 逐块降采样发屏。必须先等上一块 DMA 完成再重写分块缓冲:若先写后等,
       DMA 仍在读取时缓冲即被下一块覆盖,屏幕出现周期性横条错位(每分块 20 行一条)。 */
    const int chunks = (sy2 - sy1) / WS128_SCALE_CHUNK_LINES + 1;
    __atomic_store_n(&s_pending_chunks, chunks, __ATOMIC_RELAXED);
    for (int cy = sy1; cy <= sy2; cy += WS128_SCALE_CHUNK_LINES) {
        const int cy_end = (cy + WS128_SCALE_CHUNK_LINES - 1 < sy2) ? (cy + WS128_SCALE_CHUNK_LINES - 1) : sy2;
        xSemaphoreTake(s_chunk_dma_done, portMAX_DELAY);
        uint16_t *dst = s_chunk;
        for (int y = cy; y <= cy_end; y++) {
            const uint16_t *src_row =
                &s_shadow[(uint32_t)ws128_scale_src((uint16_t)y) * BOARD_WS_128_GC9A01_UI_RES];
            for (int x = sx1; x <= sx2; x++) {
                *dst++ = src_row[ws128_scale_src((uint16_t)x)];
            }
        }
        esp_lcd_panel_draw_bitmap(s_panel, sx1, cy, sx2 + 1, cy_end + 1, s_chunk);
    }
}

#endif /* CONFIG_OBD_BOARD_WS_128_GC9A01 */
