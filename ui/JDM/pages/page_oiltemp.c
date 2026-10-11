// ================================================================
//  page_oiltemp.c — 发动机油温(原型 04 页 OIL TEMPERATURE)
//
//  270° 细弧(r246 轨道 w6)+ 居中主读数;40..140°C。
// ================================================================

#include "widgets/jdm_alert.h"
#include "core/jdm_alerts_logic.h"
#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_draw.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_fonts.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_arc.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_readout_logic.h"
#include "widgets/jdm_status_text.h"

#include <stdio.h>

static lv_obj_t *s_arc;
static jdm_readout_t s_ro;
static lv_obj_t *s_status;
static jdm_alert_widgets_t s_alert;

static void create(lv_obj_t *page)
{
    // 轨道 r246 w6
    lv_obj_t *track = lv_arc_create(page);
    int32_t size = UIS(2 * 246 + 6);
    lv_obj_set_size(track, size, size);
    lv_obj_set_pos(track, UIS(360) - size / 2, UIS(360) - size / 2);
    lv_obj_remove_style(track, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(track, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(track, jdm_color(JDM_ROLE_PANEL), LV_PART_MAIN);
    lv_obj_set_style_arc_width(track, UIS(6), LV_PART_MAIN);
    lv_arc_set_rotation(track, 0);
    lv_arc_set_bg_angles(track, 135, 405);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);

    // 刻度:135° 起每 9° 共 31 根,外端固定 r260;major(每 5 根)向内伸至
    // r243 w4,minor r253..260 w2;≥130°C(末 4 根)红,其余 line 色
    for (int i = 0; i <= 30; i++) {
        int deg = 135 + i * 9;
        bool major = (i % 5 == 0);
        int32_t r1 = major ? 243 : 253;
        jdm_color_role_t c = (i >= 27) ? JDM_ROLE_RED : JDM_ROLE_LINE;
        jdm_draw_tick(page, 360, 360, r1, 260, deg, major ? 4 : 2, c);
    }
    // 标签 40/90/140:数字中心按原型落在 r222 同心环(art.js 坐标为
    // SVG 基线语义),垂直用字体 line_height 居中
    static const struct { int deg; const char *txt; } lbls[] = {
        {135, "40"}, {270, "90"}, {405, "140"},
    };
    const lv_font_t *num_font = jdm_font(JDM_FONT_SUPPORT);
    int32_t half_lh = num_font->line_height / 2;
    for (unsigned i = 0; i < sizeof(lbls) / sizeof(lbls[0]); i++) {
        jdm_pt_t p = jdm_draw_polar(360, 360, 222, lbls[i].deg);
        jdm_text(page, p.x, p.y - half_lh, lbls[i].txt, JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    }

    s_arc = jdm_arc_create(page, 360, 360, 246, 6);
    jdm_arc_configure(s_arc, 135, 270, 100);   // 40..140°C → 0..100

    jdm_text(page, 360, 172, "OIL TEMPERATURE", JDM_FONT_TITLE, JDM_ROLE_AMBER, JDM_TEXT_MIDDLE);

    jdm_readout_create(page, 360, 242, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "ENGINE OIL");
    jdm_readout_set_unit(&s_ro, "\xC2\xB0" "C");   // °C

    s_status = jdm_status_text_create(page, 360, 540, "WAITING");
    jdm_alert_create(page, 0, &s_alert);
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_OIL, &raw) && jdm_conn_active();

    if (valid) {
        // 40..140°C:弧 = (temp−40)/100×270
        jdm_arc_set(s_arc, raw - 40);   // 数据域 0..100,LVGL 浮点映射到角度
    } else {
        lv_obj_set_hidden(s_arc, true);
    }
    char buf[16];
    if (valid) {
        snprintf(buf, sizeof(buf), "%d", (int)raw);
    } else {
        snprintf(buf, sizeof(buf), "\xE2\x80\x94");
    }
    jdm_readout_set_value(&s_ro, buf);

    if (!valid) {
        jdm_status_text_set(s_status, "WAITING", JDM_STATUS_DISCONNECTED);
    } else if (raw >= 110) {
        jdm_status_text_set(s_status, "OIL TEMP HIGH", JDM_STATUS_WARNING);
    } else {
        jdm_status_text_set(s_status, "WARM / STABLE", JDM_STATUS_NORMAL);
    }

    jdm_alert_item_t item;
    int n = 0;
    const char *reason = NULL;
    if (valid) {
        jdm_alert_level_t lv = jdm_alerts_logic_oil(raw, &reason);
        n = jdm_alerts_logic_push(&item, n, 1, reason, lv);
    }
    jdm_readout_set_warning(&s_ro, n > 0);
    jdm_alert_set(&s_alert, &item, n);
}

const jdm_page_t page_oiltemp = {"oiltemp", create, update, 33};
