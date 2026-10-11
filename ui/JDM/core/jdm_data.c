// ================================================================
//  jdm_data.c — JDM 统一数据通道实现
// ================================================================

#include "jdm_data.h"

#include "app_obd_dsp/obd_data_cache.h"

#include <stddef.h>

static const jdm_data_meta_t s_meta[DISP_ITEM_COUNT] = {
    [DISP_ITEM_CLT]   = {"CLT", "'C"},
    [DISP_ITEM_IAT]   = {"IAT", "'C"},
    [DISP_ITEM_OIL]   = {"OIL", "'C"},
    [DISP_ITEM_LOAD]  = {"LOD", "%"},
    [DISP_ITEM_TPS]   = {"TPS", "%"},
    [DISP_ITEM_RPM]   = {"RPM", "rpm"},
    [DISP_ITEM_SPEED] = {"SPD", "km/h"},
    [DISP_ITEM_BAT]   = {"BAT", "V"},
    [DISP_ITEM_OILP]  = {"OIP", "bar"},
    [DISP_ITEM_BKT]   = {"BKT", "'C"},
    [DISP_ITEM_BOOST] = {"BST", "bar"},
    [DISP_ITEM_AFR]   = {"AFR", ""},
    [DISP_ITEM_TPMS_FL] = {"TPFL", "bar"},
    [DISP_ITEM_TPMS_FR] = {"TPFR", "bar"},
    [DISP_ITEM_TPMS_RL] = {"TPRL", "bar"},
    [DISP_ITEM_TPMS_RR] = {"TPRR", "bar"},
    [DISP_ITEM_GFORCE_LAT] = {"GFL", "g"},
    [DISP_ITEM_GFORCE_LON] = {"GFN", "g"},
    [DISP_ITEM_HEART] = {"HRT", "bpm"},
};

static const jdm_data_range_t s_range[DISP_ITEM_COUNT] = {
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
    [DISP_ITEM_BOOST] = {-10, 30, 10},   // −1.0 ~ +3.0 bar 表压(raw 0.1bar,含真空)
    [DISP_ITEM_AFR]   = {8, 22, 100},
    [DISP_ITEM_TPMS_FL] = {0, 45, 10},
    [DISP_ITEM_TPMS_FR] = {0, 45, 10},
    [DISP_ITEM_TPMS_RL] = {0, 45, 10},
    [DISP_ITEM_TPMS_RR] = {0, 45, 10},
    [DISP_ITEM_GFORCE_LAT] = {-15, 15, 100},   // ±1.50 g
    [DISP_ITEM_GFORCE_LON] = {-15, 15, 100},
    [DISP_ITEM_HEART] = {30, 200, 1},
};

bool jdm_data_read_cache(disp_item_t item, int32_t *out)
{
    if (!out || (int)item < 0 || (int)item >= (int)DISP_ITEM_COUNT) return false;

    // 各通道:先取缓存原始值,再走统一的哨兵判定(与 obd_data_cache 约定一致)
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
    case DISP_ITEM_HEART: return false;   // 数据源(BLE HR)接入前恒无效
    default: return false;
    }
    if (!present) return false;
    *out = v;
    return true;
}

const jdm_data_meta_t *jdm_data_meta(disp_item_t item)
{
    if ((int)item < 0 || (int)item >= (int)DISP_ITEM_COUNT) item = 0;
    return &s_meta[item];
}

const jdm_data_range_t *jdm_data_range(disp_item_t item)
{
    if ((int)item < 0 || (int)item >= (int)DISP_ITEM_COUNT) item = 0;
    return &s_range[item];
}
