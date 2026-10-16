// ================================================================
//  page_heart.c — 心率 / 趋势(原型 11 页 DRIVER PULSE)
//
//  数据源 BLE HR 0x180D → obd_data_cache(0 = NO SENSOR)。有效时:
//  主读数 + bpm 历史趋势(21 点)+ LOW/PEAK/1 MIN 统计(1 分钟窗);
//  出车型设定范围(heart_cfg_t)时统计值变 amber、页脚 advisory,
//  副标题切 "RANGE low / high bpm"。断连回 "SENSOR / WAITING" 态。
// ================================================================

#include "widgets/jdm_alert.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"
#include "widgets/jdm_trend.h"

#include <stdio.h>
#include <string.h>

#define HR_WIN      300     // 统计窗:1 min @ 200ms 页面更新
#define HR_TREND    21      // 趋势点数(与 create 的 max_n 一致)
#define HR_DASH     "\xE2\x80\x94"

static jdm_readout_t s_ro;
static lv_obj_t *s_sub;                 // 副标题 label(运行时改写)
static lv_obj_t *s_baseline;
static jdm_trend_t s_trend;
static lv_obj_t *s_stats[3];
static jdm_alert_widgets_t s_alert;
static heart_cfg_t s_hb;                // 创建时取车型配置

static int16_t s_hist[HR_WIN];          // bpm 历史(旧→新)
static int s_hist_n;

static void create(lv_obj_t *page)
{
    s_hb = *vehicle_profile_get_heart_cfg();

    jdm_header_create_ex(page, "DRIVER PULSE", "SENSOR / WAITING", &s_sub);

    jdm_readout_create(page, 360, 190, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "PULSE");
    jdm_readout_set_unit(&s_ro, "bpm");

    jdm_trend_create(page, 115, 440, 490, 65, HR_TREND, &s_trend);
    static const jdm_pt_t base[] = {{115, 478}, {605, 478}};
    s_baseline = jdm_draw_poly(page, base, 2, 2, JDM_ROLE_LINE);

    static const jdm_pt_t sep[] = {{112, 532}, {608, 532}};
    jdm_draw_poly(page, sep, 2, 2, JDM_ROLE_LINE);

    static const struct { int32_t x; const char *lbl; } stats[3] = {
        {168, "LOW"}, {360, "PEAK"}, {552, "1 MIN"},
    };
    for (int i = 0; i < 3; i++) {
        jdm_text(page, stats[i].x, 550, stats[i].lbl, JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
        s_stats[i] = jdm_text(page, stats[i].x, 582, HR_DASH, JDM_FONT_STATISTIC,
                              JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    jdm_alert_create(page, 0, &s_alert);
}

// 统计值颜色(原型):报警时越界值 amber、界内 text;常态 PEAK amber
static void stat_show(int i, int32_t val, bool alert)
{
    jdm_color_role_t c;
    if (alert) {
        c = (val < (int32_t)s_hb.low_bpm || val > (int32_t)s_hb.high_bpm)
                ? JDM_ROLE_AMBER : JDM_ROLE_TEXT;
    } else {
        c = (i == 1) ? JDM_ROLE_AMBER : JDM_ROLE_TEXT;
    }
    lv_obj_set_style_text_color(s_stats[i], jdm_color(c), 0);
    lv_label_set_text_fmt(s_stats[i], "%d", (int)val);
}

static void stats_reset(void)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_text_color(s_stats[i], jdm_color(JDM_ROLE_MUTED), 0);
        lv_label_set_text(s_stats[i], HR_DASH);
    }
}

static void update(void)
{
    int32_t bpm = 0;
    bool ok = jdm_data_read_cache(DISP_ITEM_HEART, &bpm) && jdm_conn_active();
    char buf[16];
    jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_HEART, bpm, ok);
    jdm_readout_set_value(&s_ro, buf);
    lv_obj_set_hidden(s_trend.line, !ok);
    lv_obj_set_hidden(s_baseline, ok);

    if (!ok) {
        s_hist_n = 0;
        stats_reset();
        if (s_sub) lv_label_set_text(s_sub, "SENSOR / WAITING");
        jdm_alert_item_t item = {0};
        jdm_alert_set(&s_alert, &item, 0);
        return;
    }

    // 1 分钟滑动窗(200ms × 300)
    if (s_hist_n < HR_WIN) {
        s_hist[s_hist_n++] = (int16_t)bpm;
    } else {
        memmove(s_hist, s_hist + 1, (HR_WIN - 1) * sizeof(s_hist[0]));
        s_hist[HR_WIN - 1] = (int16_t)bpm;
    }

    // 趋势:最近 HR_TREND 点
    int32_t tv[HR_TREND];
    int tn = s_hist_n < HR_TREND ? s_hist_n : HR_TREND;
    for (int i = 0; i < tn; i++) tv[i] = s_hist[s_hist_n - tn + i];
    jdm_trend_set(&s_trend, tv, tn);

    // 统计:LOW / PEAK / 1 MIN 均值(同一窗口)
    int32_t lo = s_hist[0], hi = s_hist[0], sum = 0;
    for (int i = 0; i < s_hist_n; i++) {
        if (s_hist[i] < lo) lo = s_hist[i];
        if (s_hist[i] > hi) hi = s_hist[i];
        sum += s_hist[i];
    }
    int32_t mean = sum / s_hist_n;

    const char *reason = NULL;
    jdm_alert_level_t lv = jdm_alerts_logic_heart(bpm, s_hb.low_bpm, s_hb.high_bpm, &reason);
    bool alert = lv != JDM_ALERT_NONE;

    stat_show(0, lo, alert);
    stat_show(1, hi, alert);
    stat_show(2, mean, alert);

    if (s_sub) {
        if (alert) {
            lv_label_set_text_fmt(s_sub, "RANGE %d / %d bpm",
                                  (int)s_hb.low_bpm, (int)s_hb.high_bpm);
        } else {
            lv_label_set_text(s_sub, "LIVE / HEART RATE");
        }
    }

    jdm_readout_set_warning(&s_ro, alert);
    jdm_alert_item_t item;
    int n = jdm_alerts_logic_push(&item, 0, 1, reason, lv);
    jdm_alert_set(&s_alert, &item, n);
}

const jdm_page_t page_heart = {"heart", create, update, 200};
