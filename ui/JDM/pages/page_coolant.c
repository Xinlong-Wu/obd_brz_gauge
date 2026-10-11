// ================================================================
//  page_coolant.c — 冷却液温度(原型 03 页)
//
//  温度柱(轨道+红区刻度)+ 右侧主读数 + 状态字;"120"/"60" 端标。
// ================================================================

#include "core/jdm_conn.h"
#include "core/jdm_data.h"
#include "core/jdm_geo.h"
#include "core/jdm_nav.h"
#include "core/jdm_text.h"
#include "core/jdm_theme.h"
#include "widgets/jdm_header.h"
#include "widgets/jdm_readout.h"
#include "widgets/jdm_status_text.h"
#include "widgets/jdm_thermal.h"

#include <stdio.h>

static jdm_readout_t s_ro;
static lv_obj_t *s_thermal;
static lv_obj_t *s_status;

static void create(lv_obj_t *page)
{
    jdm_header_create(page, "COOLANT", "ENGINE THERMAL");
    s_thermal = jdm_thermal_create(page);

    // reading('coolant',265,220) → cx = 265+160 = 425
    jdm_readout_create(page, 425, 220, JDM_RO_PRIMARY, &s_ro);
    jdm_readout_set_label(&s_ro, "WATER");
    jdm_readout_set_unit(&s_ro, "\xC2\xB0" "C");

    s_status = jdm_text(page, 420, 530, "WAITING", JDM_FONT_LABEL, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
    jdm_text(page, 112, 208, "120", JDM_FONT_SUPPORT, JDM_ROLE_RED, JDM_TEXT_MIDDLE);
    jdm_text(page, 112, 480, "60", JDM_FONT_SUPPORT, JDM_ROLE_MUTED, JDM_TEXT_MIDDLE);
}

static void update(void)
{
    int32_t raw = 0;
    bool valid = jdm_data_read_cache(DISP_ITEM_CLT, &raw) && jdm_conn_active();

    jdm_thermal_update(s_thermal, raw, valid, valid && raw >= 110);

    char buf[16];
    if (valid) {
        snprintf(buf, sizeof(buf), "%d", (int)raw);
    } else {
        snprintf(buf, sizeof(buf), "\xE2\x80\x94");
    }
    jdm_readout_set_value(&s_ro, buf);

    if (!valid) {
        lv_label_set_text(s_status, "WAITING");
        lv_obj_set_style_text_color(s_status, jdm_color(JDM_ROLE_MUTED), 0);
    } else if (raw >= 110) {
        lv_label_set_text(s_status, "TEMP HIGH");
        lv_obj_set_style_text_color(s_status, jdm_color(JDM_ROLE_RED), 0);
    } else {
        lv_label_set_text(s_status, "STABLE");
        lv_obj_set_style_text_color(s_status, jdm_color(JDM_ROLE_GREEN), 0);
    }
}

const jdm_page_t page_coolant = {"coolant", create, update, 33};
