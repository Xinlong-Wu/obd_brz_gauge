// ui_scale.c — 通用缩放输出层(实现见同名头文件)。
// 影子缓冲放 PSRAM(渲染分辨率²×2 字节),分块输出缓冲放内部 DMA 内存,
// 信号量保证复用前上次 DMA 已完成——必须先取信号量再写缓冲:先写后等
// 会覆盖在途 DMA 数据,屏幕出现每分块一条的周期性横条(WS128 踩过)。
// 缩放统一用盒式滤波:每个输出像素取其覆盖源区间的逐分量平均,
// 下采样(3:2 等)是多像素平均,上采样退化为最近邻/相邻均值。
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "bsp_obd_dsp/boards/board_api.h"
#include "bsp_obd_dsp/boards/ui_scale.h"

static const char *TAG = "ui_scale";

#define UI_SCALE_CHUNK_LINES 20
#define UI_SCALE_MAX_RES     512   // 列区间表上限(Kconfig 渲染范围 240-480)

static uint16_t *s_shadow;                        // 渲染分辨率全帧影子(PSRAM)
static uint16_t *s_chunk;                         // 面板宽×20 行输出块(内部 DMA 内存)
static SemaphoreHandle_t s_chunk_dma_done;        // 分块缓冲空闲信号量
static volatile int s_pending_chunks;             // 未完成分块数,归零转发 LVGL flush_ready
static esp_lcd_panel_handle_t s_panel;
static uint16_t s_src_res;                        // 渲染分辨率(LVGL 侧)
static uint16_t s_dst_res;                        // 面板分辨率
static board_display_flush_ready_cb_t s_flush_ready_cb;
static void *s_flush_ready_user_ctx;

/* 列采样区间表:同一 flush 内所有行复用,避免每像素两次除法 */
static int16_t s_sx0[UI_SCALE_MAX_RES];
static int16_t s_sx1[UI_SCALE_MAX_RES];

bool ui_scale_active(void)
{
    return s_shadow != NULL;
}

bool ui_scale_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
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

void ui_scale_set_flush_ready_chain(board_display_flush_ready_cb_t cb, void *user_ctx)
{
    s_flush_ready_cb = cb;
    s_flush_ready_user_ctx = user_ctx;
}

esp_err_t ui_scale_init(esp_lcd_panel_handle_t panel, uint16_t src_res, uint16_t dst_res)
{
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_ARG, TAG, "panel handle is null");
    ESP_RETURN_ON_FALSE(src_res != 0 && dst_res != 0 && src_res <= UI_SCALE_MAX_RES &&
                        dst_res <= UI_SCALE_MAX_RES, ESP_ERR_INVALID_ARG, TAG,
                        "resolutions out of range (src=%u dst=%u)", src_res, dst_res);

    if (s_shadow != NULL) {
        return (s_src_res == src_res && s_dst_res == dst_res)
                   ? ESP_OK
                   : ESP_ERR_INVALID_STATE;   // 已按其他参数初始化
    }

    s_panel = panel;
    s_src_res = src_res;
    s_dst_res = dst_res;

    s_shadow = heap_caps_malloc((uint32_t)src_res * src_res * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    s_chunk = heap_caps_malloc((uint32_t)dst_res * UI_SCALE_CHUNK_LINES * sizeof(uint16_t),
                               MALLOC_CAP_DMA);
    s_chunk_dma_done = xSemaphoreCreateBinary();
    if (s_chunk_dma_done != NULL) {
        xSemaphoreGive(s_chunk_dma_done);   // 分块缓冲初始空闲
    }
    if (s_shadow == NULL || s_chunk == NULL || s_chunk_dma_done == NULL) {
        ESP_LOGE(TAG, "buffers alloc failed: shadow=%p chunk=%p sem=%p",
                 s_shadow, s_chunk, (void *)s_chunk_dma_done);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "active: render %u -> panel %u (box filter, shadow %u B PSRAM, chunk %u B DMA)",
             (unsigned)src_res, (unsigned)dst_res,
             (unsigned)((uint32_t)src_res * src_res * sizeof(uint16_t)),
             (unsigned)((uint32_t)dst_res * UI_SCALE_CHUNK_LINES * sizeof(uint16_t)));
    return ESP_OK;
}

/** 覆盖源区间逐分量平均(兼容 LV_COLOR_16_SWAP 的存储字节序)。 */
static inline uint16_t ui_scale_avg(const uint16_t *px, int count)
{
    if (count <= 1) {
        return *px;
    }
    uint32_t r = 0, g = 0, b = 0;
    for (int i = 0; i < count; i++) {
        uint16_t v = __builtin_bswap16(px[i]);
        r += (v >> 11) & 0x1f;
        g += (v >> 5) & 0x3f;
        b += v & 0x1f;
    }
    uint16_t v = (uint16_t)(((r / count) << 11) | ((g / count) << 5) | (b / count));
    return __builtin_bswap16(v);
}

void ui_scale_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)disp;
    const int S = s_src_res;
    const int D = s_dst_res;
    const uint16_t *color_map = (const uint16_t *)px_map;   // RGB565_SWAPPED,2 字节/像素

    /* 1. LVGL 渲染缓冲行按脏区宽度紧排,先并入全帧影子 */
    const int w = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(&s_shadow[(uint32_t)y * S + area->x1],
               &color_map[(uint32_t)(y - area->y1) * w],
               (uint32_t)w * sizeof(uint16_t));
    }

    /* 2. 脏区映射到面板(floor 下界 / ceil 上界,保证覆盖) */
    const int dmax = D - 1;
    const int dx1 = (area->x1 * D) / S;
    const int dy1 = (area->y1 * D) / S;
    int dx2 = ((area->x2 + 1) * D + S - 1) / S - 1;
    int dy2 = ((area->y2 + 1) * D + S - 1) / S - 1;
    if (dx2 > dmax) dx2 = dmax;
    if (dy2 > dmax) dy2 = dmax;

    /* 3. 列采样区间表(区间为空时至少取 1 源像素 → 上采样退化最近邻) */
    for (int x = dx1; x <= dx2; x++) {
        int x0 = (x * S) / D;
        int x1 = ((x + 1) * S + D - 1) / D;
        if (x1 <= x0) x1 = x0 + 1;
        if (x1 > S) x1 = S;
        s_sx0[x] = (int16_t)x0;
        s_sx1[x] = (int16_t)x1;
    }

    /* 4. 逐块缩放发屏;先取信号量再写分块缓冲(见文件头) */
    const int chunks = (dy2 - dy1) / UI_SCALE_CHUNK_LINES + 1;
    __atomic_store_n(&s_pending_chunks, chunks, __ATOMIC_RELAXED);
    for (int cy = dy1; cy <= dy2; cy += UI_SCALE_CHUNK_LINES) {
        const int cy_end = (cy + UI_SCALE_CHUNK_LINES - 1 < dy2) ? (cy + UI_SCALE_CHUNK_LINES - 1) : dy2;
        xSemaphoreTake(s_chunk_dma_done, portMAX_DELAY);
        uint16_t *dst = s_chunk;
        for (int y = cy; y <= cy_end; y++) {
            int y0 = (y * S) / D;
            int y1 = ((y + 1) * S + D - 1) / D;
            if (y1 <= y0) y1 = y0 + 1;
            if (y1 > S) y1 = S;
            const int rows = y1 - y0;
            const uint16_t *base = &s_shadow[(uint32_t)y0 * S];
            for (int x = dx1; x <= dx2; x++) {
                const int cols = s_sx1[x] - s_sx0[x];
                if (cols == 1 && rows == 1) {
                    *dst++ = base[s_sx0[x]];
                } else {
                    uint32_t r = 0, g = 0, b = 0;
                    for (int sy = 0; sy < rows; sy++) {
                        const uint16_t *row = base + (uint32_t)sy * S + s_sx0[x];
                        for (int sx = 0; sx < cols; sx++) {
                            uint16_t v = __builtin_bswap16(row[sx]);
                            r += (v >> 11) & 0x1f;
                            g += (v >> 5) & 0x3f;
                            b += v & 0x1f;
                        }
                    }
                    const int n = rows * cols;
                    uint16_t v = (uint16_t)(((r / n) << 11) | ((g / n) << 5) | (b / n));
                    *dst++ = __builtin_bswap16(v);
                }
            }
        }
        esp_lcd_panel_draw_bitmap(s_panel, dx1, cy, dx2 + 1, cy_end + 1, s_chunk);
    }
}
