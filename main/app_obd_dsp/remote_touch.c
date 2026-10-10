// remote_touch.c — WiFi 远程触摸注入(实现见同名头文件)。
//
// 为什么设备端保留一个小队列:LVGL 每 ~30ms 才采样一次 indev,快速点按
// (按下+抬起 <30ms)在"单槽最新值"方案下只看得到抬起、点击会丢;8 深度
// 事件队列保证按下→移动→抬起的顺序性,read_cb 每次 poll 回放一个事件,
// 队列空时保持上一状态(语义等同手指停在原地/未抬起)。
#include "sdkconfig.h"

#if CONFIG_OBD_REMOTE_TOUCH

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "app_obd_dsp/remote_touch.h"
#include "app_obd_dsp/screen_capture.h"

static const char *TAG = "remote_touch";

#define REMOTE_TOUCH_QUEUE_LEN   8
#define REMOTE_TOUCH_NORM_MAX    10000
#define REMOTE_TOUCH_STALE_US    2000000   // 按下态 2s 无事件 → 合成抬起

static void remote_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data);

typedef struct {
    int x, y;            // 归一化 0-10000
    bool pressed;
} remote_touch_ev_t;

static QueueHandle_t s_queue;
static lv_indev_state_t s_state = LV_INDEV_STATE_RELEASED;
static lv_point_t s_point;
static int64_t s_last_event_us;
static bool s_ready;

esp_err_t remote_touch_register(lv_display_t *disp)
{
    if (s_ready) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_INVALID_ARG, TAG, "disp is null");

    s_queue = xQueueCreate(REMOTE_TOUCH_QUEUE_LEN, sizeof(remote_touch_ev_t));
    ESP_RETURN_ON_FALSE(s_queue != NULL, ESP_ERR_NO_MEM, TAG, "queue alloc failed");

    lv_indev_t *indev = lv_indev_create();   // v9:不再有静态 drv 结构
    ESP_RETURN_ON_FALSE(indev != NULL, ESP_ERR_NO_MEM, TAG, "indev create failed");
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, remote_touch_read_cb);
    lv_indev_set_display(indev, disp);

    s_ready = true;
    ESP_LOGI(TAG, "virtual pointer indev registered (remote touch via /touch)");
    return ESP_OK;
}

bool remote_touch_ready(void)
{
    return s_ready;
}

void remote_touch_feed(int x_norm, int y_norm, bool pressed)
{
    if (s_queue == NULL) {
        return;
    }
    remote_touch_ev_t ev = {
        .x = x_norm < 0 ? 0 : (x_norm > REMOTE_TOUCH_NORM_MAX ? REMOTE_TOUCH_NORM_MAX : x_norm),
        .y = y_norm < 0 ? 0 : (y_norm > REMOTE_TOUCH_NORM_MAX ? REMOTE_TOUCH_NORM_MAX : y_norm),
        .pressed = pressed,
    };
    xQueueSend(s_queue, &ev, 0);   // 满则丢弃最旧不可得,下一采样补偿
}

/** LVGL indev 轮询(LVGL 锁内):回放一个事件;空队列保持上一状态。 */
void remote_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    remote_touch_ev_t ev;
    if (xQueueReceive(s_queue, &ev, 0) == pdTRUE) {
        s_point.x = (int16_t)((int32_t)ev.x * screen_capture_res() / REMOTE_TOUCH_NORM_MAX);
        s_point.y = (int16_t)((int32_t)ev.y * screen_capture_res() / REMOTE_TOUCH_NORM_MAX);
        s_state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        s_last_event_us = esp_timer_get_time();
    } else if (s_state == LV_INDEV_STATE_PRESSED &&
               esp_timer_get_time() - s_last_event_us > REMOTE_TOUCH_STALE_US) {
        s_state = LV_INDEV_STATE_RELEASED;   // 客户端断连/停发,合成抬起防卡键
    }
    data->point = s_point;
    data->state = s_state;
}

#endif /* CONFIG_OBD_REMOTE_TOUCH */
