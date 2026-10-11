#pragma once
// ================================================================
//  jdm_readout_logic.h — readout 数值文本格式化纯逻辑(static inline)
//
//  通道 raw 值 → 显示文本,约定与数据层除数一致:
//    BAT  mV→"14.2"(1 位小数)   OILP  0.1bar→"3.8"
//    BKT  0.1°C→"60"(整数)     BOOST 0.1bar→"-0.6"/"1.2"(带符号)
//    AFR  ×100→"14.7"           其余整数
//  无效一律 "--"。tests/ 直接编译断言。
// ================================================================

#include <stdio.h>

#include "core/jdm_data.h"

/** 按通道格式化 raw 值。invalid 时写 "--"。返回 buf(便于嵌套调用)。 */
static inline const char *jdm_readout_logic_format(char *buf, unsigned len,
                                                   disp_item_t item, int32_t raw,
                                                   bool valid)
{
    if (!buf || len == 0) return buf;
    if (!valid) {
        snprintf(buf, len, "\xE2\x80\x94");   // "—"(em dash,设计断连语义)
        return buf;
    }
    switch (item) {
    case DISP_ITEM_BAT:   // mV → V,1 位小数(截断,同旧管线)
        snprintf(buf, len, "%d.%d", (int)(raw / 1000), (int)((raw % 1000) / 100));
        break;
    case DISP_ITEM_OILP:
    case DISP_ITEM_TPMS_FL:
    case DISP_ITEM_TPMS_FR:
    case DISP_ITEM_TPMS_RL:
    case DISP_ITEM_TPMS_RR: {   // 0.1 bar → 1 位小数
        int32_t a = (raw < 0) ? -raw : raw;
        snprintf(buf, len, "%s%d.%d", (raw < 0) ? "-" : "", (int)(a / 10), (int)(a % 10));
        break;
    }
    case DISP_ITEM_BKT:   // 0.1°C → 整数
        snprintf(buf, len, "%d", (int)(raw / 10));
        break;
    case DISP_ITEM_BOOST: {   // 0.01 bar 表压,可负(原型 toFixed(2);raw 0.1bar 低位补 0)
        int32_t a = (raw < 0) ? -raw * 10 : raw * 10;
        snprintf(buf, len, "%s%d.%02d", (raw < 0) ? "-" : "", (int)(a / 100), (int)(a % 100));
        break;
    }
    case DISP_ITEM_AFR:   // ×100 → 1 位小数
        snprintf(buf, len, "%d.%d", (int)(raw / 100), (int)((raw % 100) / 10));
        break;
    case DISP_ITEM_GFORCE_LAT:
    case DISP_ITEM_GFORCE_LON: {   // ×100 g → 2 位小数
        int32_t a = (raw < 0) ? -raw : raw;
        snprintf(buf, len, "%s%d.%02d", (raw < 0) ? "-" : "", (int)(a / 100), (int)(a % 100));
        break;
    }
    default:
        snprintf(buf, len, "%d", (int)raw);
        break;
    }
    return buf;
}
