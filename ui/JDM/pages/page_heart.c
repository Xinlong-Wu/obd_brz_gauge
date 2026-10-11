// ================================================================
//  page_heart.c — 心率 / 趋势(原型 11 页 DRIVER PULSE)
//
//  数据源未接入(BLE HR 0x180D):恒断连态 — "NO SENSOR"、
//  基线横线、三统计值为 —。数据源接入后 trend/统计自动启用。
// ================================================================

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

static jdm_readout_t s_ro;
static lv_obj_t *s_sub;   // 预留:运行时副标题
static lv_obj_t *s_baseline;
static jdm_trend_t s_trend;
static lv_obj_t *s_stats[3];

static void create(lv_obj_t *page)
{
    // 数据源接入后副标题切 "LIVE / HEART RATE";当前恒等待
    jdm_header_create(page, "DRIVER PULSE", "SENSOR / WAITING");
    (void)s_sub;

    jdm_readout_create(page, 360, 190, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "PULSE");
    jdm_readout_set_unit(&s_ro, "bpm");

    jdm_trend_create(page, 115, 440, 490, 65, 21, &s_trend);
    static const jdm_pt_t base[] = {{115, 478}, {605, 478}};
    s_baseline = jdm_draw_poly(page, base, 2, 2, JDM_ROLE_LINE);

    static const jdm_pt_t sep[] = {{112, 532}, {608, 532}};
    jdm_draw_poly(page, sep, 2, 2, JDM_ROLE_LINE);

    static const struct { int32_t x; const char *lbl; } stats[3] = {
        {168, "LOW"}, {360, "PEAK"}, {552, "1 MIN"},
    };
    for (int i = 0; i < 3; i++) {
        jdm_text(page, stats[i].x, 550, stats[i].lbl, JDM_FONT_SMALL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
        s_stats[i] = jdm_text(page, stats[i].x, 582, "\xE2\x80\x94", JDM_FONT_STATISTIC,
                              (i == 1) ? JDM_ROLE_AMBER : JDM_ROLE_TEXT, JDM_TEXT_MIDDLE);
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
}

const jdm_page_t page_heart = {"heart", create, update, 200};
