// ================================================================
//  ui_disp_item.c — configurable data-item system implementation (extracted from ui.c)
// ================================================================

#include "ui_disp_item.h"
#include "ui_disp_item_logic.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "app_obd_dsp/obd_data_cache.h"

#include <stdio.h>
#include <string.h>

#define DISP_ITEM_ALARM_COOLDOWN_MS 30000U

const disp_item_meta_t s_disp_meta[DISP_ITEM_COUNT] = {
    {"CLT", "'C", 0x44AAFF},
    {"IAT", "'C", 0x44FF88},
    {"OIL", "'C", 0xFF7722},
    {"LOD", "%", 0xFFCC00},
    {"TPS", "%", 0xFF8844},
    {"RPM", "rpm", 0x66CCFF},
    {"SPD", "km/h", 0xFFFFFF},
    {"BAT", "V", 0xAACCFF},
    {"OIP", "bar", 0xFFD166},
    {"BKT", "'C", 0xFF5A5A},
    {"BST", "bar", 0x00DD88},
    {"AFR", "", 0xFFAA00},
    {"TPFL", "bar", 0x66FF99},   // M3 扩展通道:胎压四轮(0.1bar)
    {"TPFR", "bar", 0x66FF99},
    {"TPRL", "bar", 0x66FF99},
    {"TPRR", "bar", 0x66FF99},
    {"GFL", "g", 0xFFC0CB},      // G 力纵/横(0.01g)
    {"GFN", "g", 0xFFC0CB},
};

// Needle range per data item: nmin/nmax in natural units (used for both scale labels and needle position);
// div converts the cached raw value to natural units (BAT: mV→V, OILP/BKT: 0.1 units → integer).
const needle_scale_meta_t s_needle_scale_meta[DISP_ITEM_COUNT] = {
    [DISP_ITEM_CLT]   = {-20, 130, 1},
    [DISP_ITEM_IAT]   = {-20, 100, 1},
    [DISP_ITEM_OIL]   = {-20, 160, 1},
    [DISP_ITEM_LOAD]  = {0, 100, 1},
    [DISP_ITEM_TPS]   = {0, 100, 1},
    [DISP_ITEM_RPM]   = {0, 8000, 1},
    [DISP_ITEM_SPEED] = {0, 240, 1},
    [DISP_ITEM_BAT]   = {8, 16, 1000},
    [DISP_ITEM_OILP]  = {0, 10, 10},
    [DISP_ITEM_BKT]   = {0, 800, 10},
    [DISP_ITEM_BOOST] = {0, 20, 1},   // range in 0.1bar: 0 ~ +2.0 bar gauge pressure (negative pressure not displayed)
    [DISP_ITEM_AFR]   = {8, 22, 100}, // range 8.0~22.0:1, raw value ×100
    [DISP_ITEM_TPMS_FL] = {0, 45, 10},   // 0~4.5 bar, raw ×10
    [DISP_ITEM_TPMS_FR] = {0, 45, 10},
    [DISP_ITEM_TPMS_RL] = {0, 45, 10},
    [DISP_ITEM_TPMS_RR] = {0, 45, 10},
    [DISP_ITEM_GFORCE_LAT] = {-15, 15, 100},  // ±1.50 g, raw ×100
    [DISP_ITEM_GFORCE_LON] = {-15, 15, 100},
};

bool disp_item_read_value(disp_item_t item,
                          int16_t clt, int16_t iat, int16_t oil,
                          int16_t load_pct, int16_t tps, int32_t bat_mv,
                          int16_t oilp_x10, int16_t brake_x10,
                          uint16_t rpm, uint16_t speed, int16_t boost_x10,
                          int16_t afr_x100,
                          int32_t *out)
{
    if (!out) return false;
    switch (item) {
        case DISP_ITEM_CLT: if (clt > -40) { *out = clt; return true; } return false;
        case DISP_ITEM_IAT: if (iat > -40) { *out = iat; return true; } return false;
        case DISP_ITEM_OIL: if (oil > -41) { *out = oil; return true; } return false;
        case DISP_ITEM_LOAD: if (load_pct >= 0) { *out = load_pct; return true; } return false;
        case DISP_ITEM_TPS: if (tps >= 0) { *out = tps; return true; } return false;
        case DISP_ITEM_RPM: *out = rpm; return true;
        case DISP_ITEM_SPEED: *out = speed; return true;
        case DISP_ITEM_BAT: if (bat_mv > 0) { *out = bat_mv; return true; } return false;
        case DISP_ITEM_OILP: if (oilp_x10 >= 0) { *out = oilp_x10; return true; } return false;
        case DISP_ITEM_BKT: if (brake_x10 > -1000) { *out = brake_x10; return true; } return false;
        case DISP_ITEM_BOOST: if (boost_x10 != -32768) { *out = boost_x10; return true; } return false;
        case DISP_ITEM_AFR: if (afr_x100 >= 800 && afr_x100 <= 2200) { *out = afr_x100; return true; } return false;
        default: return false;
    }
}

// 统一通道读取(M3):任意 disp_item_t 直接读数据缓存,覆盖 ZC6 扩展通道。
// 有效性与 disp_item_read_value 的哨兵规则一致。
bool ui_disp_item_read_cache(disp_item_t item, int32_t *out)
{
    if (!out || (int)item < 0 || (int)item >= (int)DISP_ITEM_COUNT) return false;

    // 各通道:先取缓存原始值,再走统一的哨兵判定
    int32_t v = 0;
    bool present = false;
    switch (item) {
    case DISP_ITEM_CLT:   v = obd_data_get_coolant_temp(); present = v > -40; break;
    case DISP_ITEM_IAT:   v = obd_data_get_intake_temp();   present = v > -40;  break;
    case DISP_ITEM_OIL:   v = obd_data_get_oil_temp();      present = v > -100; break;
    case DISP_ITEM_LOAD:  v = obd_data_get_load_pct();      present = v >= 0;   break;
    case DISP_ITEM_TPS:   v = obd_data_get_tps();           present = v >= 0;   break;
    case DISP_ITEM_RPM:   v = obd_data_get_rpm();           present = true;     break;
    case DISP_ITEM_SPEED: v = obd_data_get_speed();         present = true;     break;
    case DISP_ITEM_BAT:   v = obd_data_get_bat_mv();        present = v > 0;    break;
    case DISP_ITEM_OILP:  v = obd_data_get_oil_pressure_x10(); present = v >= 0; break;
    case DISP_ITEM_BKT:   v = obd_data_get_brake_temp_x10();   present = v > -1000; break;
    case DISP_ITEM_BOOST: v = obd_data_get_boost_x10();     present = v != -32768; break;
    case DISP_ITEM_AFR:   v = obd_data_get_afr_x100();      present = v >= 800 && v <= 2200; break;
    case DISP_ITEM_TPMS_FL: v = obd_data_get_tpms_bar_x10(0); present = v >= 0; break;
    case DISP_ITEM_TPMS_FR: v = obd_data_get_tpms_bar_x10(1); present = v >= 0; break;
    case DISP_ITEM_TPMS_RL: v = obd_data_get_tpms_bar_x10(2); present = v >= 0; break;
    case DISP_ITEM_TPMS_RR: v = obd_data_get_tpms_bar_x10(3); present = v >= 0; break;
    case DISP_ITEM_GFORCE_LAT: v = obd_data_get_gforce_lat_x100(); present = v != -32768; break;
    case DISP_ITEM_GFORCE_LON: v = obd_data_get_gforce_lon_x100(); present = v != -32768; break;
    default: return false;
    }
    if (!present) return false;
    *out = v;
    return true;
}

int32_t disp_item_sweep_value(disp_item_t item, float r){
    switch (item) {
        case DISP_ITEM_CLT:
        case DISP_ITEM_IAT:
        case DISP_ITEM_OIL:
            return (int32_t)(120.0f * r);
        case DISP_ITEM_LOAD:
        case DISP_ITEM_TPS:
            return (int32_t)(100.0f * r);
        case DISP_ITEM_RPM:
            return (int32_t)(8000.0f * r);   // = SWEEP_RPM_PEAK
        case DISP_ITEM_SPEED:
            return (int32_t)(999.0f * r);    // = SWEEP_SPEED_PEAK
        case DISP_ITEM_BAT:
            return (int32_t)(12000.0f + 2400.0f * r); // 12.0~14.4V
        case DISP_ITEM_OILP:
            return (int32_t)(100.0f * r); // 0.0~10.0bar (x10)
        case DISP_ITEM_BKT:
            return (int32_t)(600.0f * r); // 0.0~60.0'C (x10)
        case DISP_ITEM_BOOST:
            return (int32_t)(20.0f * r); // 0.0~2.0bar gauge pressure (x10)
        case DISP_ITEM_AFR:
            return (int32_t)(800.0f + 1400.0f * r); // 8.0~22.0:1 (x100)
        default:
            return 0;
    }
}

void disp_item_set_text(lv_obj_t *label, disp_item_t item, int32_t value, bool valid)
{
    char text[32];

    if (!label) return;
    if (!valid) {
        snprintf(text, sizeof(text), "--");
    } else if (item == DISP_ITEM_BAT) {
        snprintf(text, sizeof(text), "%d.%d", (int)(value / 1000), (int)((value % 1000) / 100));
    } else if (item == DISP_ITEM_OILP) {
        int32_t abs_val = (value < 0) ? -value : value;
        snprintf(text, sizeof(text), "%d.%d", (int)(value / 10), (int)(abs_val % 10));
    } else if (item == DISP_ITEM_BKT) {
        snprintf(text, sizeof(text), "%ld", (long)(value / 10));
    } else if (item == DISP_ITEM_BOOST) {
        // gauge pressure can be negative (vacuum); show signed with one decimal, e.g. -0.6 / 1.2
        int32_t a = (value < 0) ? -value : value;
        snprintf(text, sizeof(text), "%s%d.%d", (value < 0) ? "-" : "", (int)(a / 10), (int)(a % 10));
    } else if (item == DISP_ITEM_AFR) {
        // AFR: raw value ×100, displayed as 14.7 (one decimal, same width as BAT/BOOST)
        snprintf(text, sizeof(text), "%d.%d", (int)(value / 100), (int)((value % 100) / 10));
    } else {
        snprintf(text, sizeof(text), "%ld", (long)value);
    }

    if (strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

static inline bool ui_disp_color32_eq(lv_color32_t a, lv_color32_t b)
{
    return a.red == b.red && a.green == b.green && a.blue == b.blue;
}
static inline bool ui_disp_color32_neq(lv_color32_t a, lv_color32_t b)
{
    return !ui_disp_color32_eq(a, b);
}

void disp_item_set_value_color(lv_obj_t *label, disp_item_t item, int32_t value, bool valid)
{
    if (!label) return;

    int16_t thr = nvs_chart_alarm_get((uint8_t)item);   // raw-value units; 32767=disabled
    lv_color_t color = (valid && value >= (int32_t)thr) ? lv_color_hex(0xFF4D4D) : lv_color_hex(0xFFFFFF);
    if (ui_disp_color32_neq(lv_color_to_32(lv_obj_get_style_text_color(label, LV_PART_MAIN), LV_OPA_COVER), lv_color_to_32(color, LV_OPA_COVER))) {
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    }
}

static void disp_item_set_value_color_throttled(lv_obj_t *label, disp_item_t item, int32_t value, bool valid)
{
    static uint32_t s_last_alarm_ms[DISP_ITEM_COUNT] = {0};

    if (!label) return;

    int16_t thr = nvs_chart_alarm_get((uint8_t)item);   // raw-value units; 32767=disabled
    bool over_threshold = ui_disp_item_alarm_over_threshold(thr, value, valid);
    bool use_cooldown = (item == DISP_ITEM_OILP || item == DISP_ITEM_BKT);
    lv_color_t color = lv_color_hex(0xFFFFFF);

    if (over_threshold) {
        color = lv_color_hex(0xFF4D4D);
        if (use_cooldown) {
            uint32_t now_ms = lv_tick_get();
            lv_color_t current_color = lv_obj_get_style_text_color(label, LV_PART_MAIN);
            bool already_red = ui_disp_color32_eq(lv_color_to_32(current_color, LV_OPA_COVER), lv_color_to_32(lv_color_hex(0xFF4D4D), LV_OPA_COVER));

            if (already_red) {
                if (s_last_alarm_ms[item] == 0) s_last_alarm_ms[item] = now_ms;
            } else {
                uint32_t last_ms = s_last_alarm_ms[item];
                if (last_ms != 0 && (uint32_t)(now_ms - last_ms) < DISP_ITEM_ALARM_COOLDOWN_MS) {
                    color = current_color;
                } else {
                    s_last_alarm_ms[item] = now_ms;
                }
            }
        }
    }

    if (ui_disp_color32_neq(lv_color_to_32(lv_obj_get_style_text_color(label, LV_PART_MAIN), LV_OPA_COVER), lv_color_to_32(color, LV_OPA_COVER))) {
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    }
}

void disp_item_update(int32_t *state, lv_obj_t *label, disp_item_t item,
                      int32_t raw, bool valid, int32_t threshold)
{
    bool was_placeholder;
    bool color_dirty = false;

    if (!state || !label) return;
    int32_t previous = *state;
    was_placeholder = (strcmp(lv_label_get_text(label), "--") == 0);
    if (valid) {
        // RPM is output directly, without the +1/+1 stepping animation: large range and fast changes — smoothing would just look "stuck"
        *state = (item == DISP_ITEM_RPM) ? raw : ui_disp_item_anim_step_i32(*state, raw, threshold);
    }
    // invalid: keep *state unchanged, avoiding a climb from 0 when data returns
    // Only rebuild the label text when the rendered value actually changed; color follows
    // the same dirty path to avoid redundant style invalidations.
    if (!valid) {
        if (!was_placeholder) {
            disp_item_set_text(label, item, *state, false);
            color_dirty = true;
        }
    } else if (was_placeholder || *state != previous) {
        disp_item_set_text(label, item, *state, valid);
        color_dirty = true;
    }
    if (color_dirty || (valid && (item == DISP_ITEM_OILP || item == DISP_ITEM_BKT))) {
        disp_item_set_value_color_throttled(label, item, *state, valid);
    }
}

const char *ui_disp_item_name(uint8_t item)
{
    if (item >= DISP_ITEM_COUNT) return "";
    return s_disp_meta[item].name;
}

const char *ui_disp_item_unit(uint8_t item)
{
    if (item >= DISP_ITEM_COUNT) item = 0;
    return s_disp_meta[item].unit;
}

uint32_t ui_disp_item_color(uint8_t item)
{
    if (item >= DISP_ITEM_COUNT) item = 0;
    return s_disp_meta[item].color;
}

void ui_disp_item_range(uint8_t item, int32_t *nmin, int32_t *nmax, int32_t *div)
{
    if (item >= DISP_ITEM_COUNT) item = 0;
    const needle_scale_meta_t *ns = &s_needle_scale_meta[item];
    if (nmin) *nmin = ns->nmin;
    if (nmax) *nmax = ns->nmax;
    if (div)  *div  = ns->div;
}

const needle_scale_meta_t *ui_disp_item_scale(disp_item_t item)
{
    return &s_needle_scale_meta[item];
}
