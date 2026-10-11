// ================================================================
//  jdm_nav.c — JDM 页面导航实现
// ================================================================

#include "jdm_nav.h"

#include <string.h>

#include "core/jdm_conn.h"
#include "core/jdm_geo.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_shell.h"

#include "esp_log.h"

static const char *TAG = "jdm_nav";

static const jdm_page_t *const *s_pages;
static uint8_t s_page_count;
static uint8_t s_active;
static lv_obj_t *s_root;        // 全屏根容器(挂在默认屏上)
static lv_obj_t *s_page_obj;    // 当前页容器
static lv_obj_t *s_status;      // 断连状态文字(壳层底部,16 页共用)
static lv_timer_t *s_refresher;

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    if (s_pages && s_pages[s_active]->update) {
        s_pages[s_active]->update();
    }
    // 壳层断连状态(原型 shell(status)):断连时 (360,658) 显示
    // NO SIGNAL(琥珀);心率页传感器独立,显示 NO SENSOR(muted)。
    // jdm_conn.h 的 disconnected 语义在此落地。
    bool conn = jdm_conn_active();
    lv_obj_set_hidden(s_status, conn);
    if (!conn) {
        bool heart = strcmp(s_pages[s_active]->name, "heart") == 0;
        lv_label_set_text(s_status, heart ? "NO SENSOR" : "NO SIGNAL");
        lv_obj_set_style_text_color(s_status,
                                    jdm_color(heart ? JDM_ROLE_MUTED
                                                    : JDM_ROLE_AMBER), 0);
    }
}

static void gesture_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    /* LVGL 9.6 方向化手势事件(经对象链冒泡到 root) */
    if (code == LV_EVENT_GESTURE_LEFT) {
        jdm_nav_step(+1);
    } else if (code == LV_EVENT_GESTURE_RIGHT) {
        jdm_nav_step(-1);
    }
}

static void page_build(uint8_t idx)
{
    if (s_page_obj) {
        // 事件回调里可能触发切页:同步删除会悬空调用链上的事件目标,异步删
        lv_obj_delete_async(s_page_obj);
        s_page_obj = NULL;
    }
    s_active = idx;
    s_page_obj = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_page_obj);
    lv_obj_set_pos(s_page_obj, 0, 0);
    lv_obj_set_size(s_page_obj, UIS(720), UIS(720));
    lv_obj_clear_flag(s_page_obj, LV_OBJ_FLAG_SCROLLABLE);

    const jdm_page_t *p = s_pages[idx];
    if (p->create) {
        p->create(s_page_obj);
    }
    uint32_t period = p->update_period_ms ? p->update_period_ms
                                          : JDM_NAV_UPDATE_PERIOD_DEFAULT;
    if (s_refresher) {
        lv_timer_set_period(s_refresher, period);
    }
    // 首帧立即刷一次,避免空数据页闪现
    if (p->update) {
        p->update();
    }
    ESP_LOGD(TAG, "page -> %s", p->name);
}

void jdm_nav_init(void)
{
    // 导航自建新屏(不叠在 splash 上);圆外四角置黑(面板边框区域)
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_screen_load(scr);

    s_root = lv_obj_create(scr);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, UIS(720), UIS(720));
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    // 空白处按下时 act_obj 需要"可点对象"才成立,root 可点兜底,
    // 手势事件再经对象链冒泡到 screen
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE);
    // 表盘壳层(face 底圆 + r342 外框线 + 12 点琥珀定位标)建在 root
    // 最底层,16 页共用、切页不重建;原型 shell() 每页必含。页面直接
    // 建在页容器上,壳层返回的 content 层这里用不到,删掉免吃点击。
    lv_obj_delete(jdm_shell_build(s_root));
    // 断连状态文字(壳层 (360,658);见 refresh_cb)
    s_status = jdm_text(s_root, 360, 658, "", JDM_FONT_SUPPORT,
                        JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);
    lv_obj_set_hidden(s_status, true);
    // 手势挂 screen:act_obj 无论落在哪个子控件,冒泡终点都是它
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE_LEFT, NULL);
    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE_RIGHT, NULL);

    s_refresher = lv_timer_create(refresh_cb, JDM_NAV_UPDATE_PERIOD_DEFAULT, NULL);
}

void jdm_nav_set_pages(const jdm_page_t *const *pages, uint8_t count)
{
    s_pages = pages;
    s_page_count = count;
    if (count > 0) {
        page_build(0);
    }
}

void jdm_nav_goto(uint8_t idx)
{
    if (!s_pages || s_page_count == 0) return;
    page_build((uint8_t)(idx % s_page_count));
}

bool jdm_nav_step(int dir)
{
    if (!s_pages || s_page_count == 0) return false;
    int next = (s_active + dir + s_page_count) % s_page_count;
    page_build((uint8_t)next);
    return true;
}

uint8_t jdm_nav_active(void)
{
    return s_active;
}
