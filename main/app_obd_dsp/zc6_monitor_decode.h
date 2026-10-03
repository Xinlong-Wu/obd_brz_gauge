#pragma once
// ================================================================
//  zc6_monitor_decode.h — ZC6(BRZ/GR86 一代)CAN 监听帧载荷解码
//
//  纯逻辑(static inline,零依赖),输入是 zc6_can_monitor_parse_line()
//  已切好的 8 字节载荷,输出整数工程值。移植自 Hokori23/obd_brz_gauge
//  的 zc6_{gear,gforce,tpms}_*_decode.h:三个复制粘贴的行级 tokenizer
//  与本仓库 can_monitor_parse_line_fast() 重复,合并后只保留载荷解码。
//
//  帧来源(ZN/C6 CAN 车型的 ATMA 监听总线,见 ft86 gen1 逆向文档):
//    0x141 挡位直读:payload[6] & 0x0F —— 0=N,1..6=前进挡,7=R
//    0x0D0 G 力:payload[6] 纵向 ×0.2g,payload[7] 横向 ×-0.1g(int8 原始)
//    0x6E2 胎压:payload[3..6] 各轮,原值 = 字节>>1,单位三种可能,
//         由 zc6_tpms_auto_resolve_scale() 用"冷胎压力应落在 1.4~3.6 bar"
//         的启发式自动判定(判定结果在调用方缓存,粘到下次重连)
// ================================================================

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 挡位直读(0x141):成功时 *gear_out 为 0=N / 1..6=前进挡 / -1=R。 */
static inline bool zc6_gear_decode_141_payload(const uint8_t payload[8], int8_t *gear_out)
{
    if (payload == NULL || gear_out == NULL) return false;

    uint8_t raw = (uint8_t)(payload[6] & 0x0Fu);
    if (raw <= 6u) {
        *gear_out = (int8_t)raw;          // 0=N,1..6=GEAR_1..6
        return true;
    }
    if (raw == 7u) {
        *gear_out = -1;                   // R(obd_data_cache 的挡位表示)
        return true;
    }
    return false;                          // 8..15:无效编码
}

/** G 力(0x0D0):输出 0.01g 整数(lat=纵向,lon=横向;int8 原值 ×20 / ×-10)。 */
static inline bool zc6_gforce_decode_0d0_payload(const uint8_t payload[8],
                                                 int16_t *lat_x100_out,
                                                 int16_t *lon_x100_out)
{
    if (payload == NULL || lat_x100_out == NULL || lon_x100_out == NULL) return false;

    *lat_x100_out = (int16_t)((int8_t)payload[6]) * 20;
    *lon_x100_out = (int16_t)((int8_t)payload[7]) * -10;
    return true;
}

/** 胎压原值的单位(0x6E2 原始字节不带单位标识,自动判定)。 */
typedef enum {
    ZC6_TPMS_SCALE_AUTO = 0,   // 未判定
    ZC6_TPMS_SCALE_PSI,        // 原值 ×0.01 PSI → 6.8948 kPa/0.01PSI
    ZC6_TPMS_SCALE_BAR,        // 原值 ×0.1 bar
    ZC6_TPMS_SCALE_KPA,        // 原值 ×0.05 kPa
} zc6_tpms_scale_t;

/** 单位换算:原值 → 0.1 bar 整数(AUTO 返回 -1 表示不可用)。 */
static inline int16_t zc6_tpms_pressure_bar_x10(uint8_t raw_half, zc6_tpms_scale_t scale)
{
    switch (scale) {
    case ZC6_TPMS_SCALE_PSI: return (int16_t)(((uint32_t)raw_half * 68948u + 50000u) / 100000u); // ×0.68948 bar
    case ZC6_TPMS_SCALE_BAR: return (int16_t)raw_half;                                           // 原值即 0.1 bar
    case ZC6_TPMS_SCALE_KPA: return (int16_t)((raw_half + 1) / 2);                              // ×0.05 bar → ×0.5 取整
    default:                 return -1;
    }
}

/**
 * 单位自动判定:冷胎压力合理区间 [1.4, 3.6] bar,对三种单位各算一轮
 * "离 2.35 bar 典型值有多近"的得分,全轮落在区间内且得分最小者胜。
 * payload[3..6] 原值为 0 的轮(未安装/未发包)跳过;全部为 0 判定失败
 * 返回 AUTO。
 */
static inline zc6_tpms_scale_t zc6_tpms_auto_resolve_scale(const uint8_t payload[8])
{
    static const zc6_tpms_scale_t candidates[] = {
        ZC6_TPMS_SCALE_PSI, ZC6_TPMS_SCALE_BAR, ZC6_TPMS_SCALE_KPA,
    };
    float best_score = 1000000.0f;
    zc6_tpms_scale_t best = ZC6_TPMS_SCALE_AUTO;

    for (size_t c = 0; c < sizeof(candidates) / sizeof(candidates[0]); c++) {
        float score = 0.0f;
        int valid = 0;
        bool reject = false;

        for (size_t i = 3; i <= 6; i++) {
            uint8_t raw = (uint8_t)(payload[i] >> 1);
            if (raw == 0u) continue;

            // 换算到 bar 检查区间(复用整数换算的浮点等价式)
            float bar;
            switch (candidates[c]) {
            case ZC6_TPMS_SCALE_PSI: bar = raw * 0.068948f; break;
            case ZC6_TPMS_SCALE_BAR: bar = raw * 0.1f;      break;
            default:                 bar = raw * 0.05f;     break;
            }
            if (bar < 1.4f || bar > 3.6f) { reject = true; break; }

            score += (bar > 2.35f) ? (bar - 2.35f) : (2.35f - bar);
            valid++;
        }

        if (!reject && valid > 0 && score < best_score) {
            best_score = score;
            best = candidates[c];
        }
    }
    return best;
}

/** 胎压(0x6E2):按已知单位解出四轮 0.1 bar;原值为 0 的轮写 -1(无效)。 */
static inline bool zc6_tpms_decode_6e2_payload(const uint8_t payload[8],
                                               zc6_tpms_scale_t scale,
                                               int16_t bar_x10_out[4])
{
    if (payload == NULL || bar_x10_out == NULL || scale == ZC6_TPMS_SCALE_AUTO) return false;

    for (int w = 0; w < 4; w++) {
        uint8_t raw = (uint8_t)(payload[3 + w] >> 1);
        bar_x10_out[w] = (raw != 0u) ? zc6_tpms_pressure_bar_x10(raw, scale) : -1;
    }
    return true;
}
