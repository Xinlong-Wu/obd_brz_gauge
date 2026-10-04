// ================================================================
//  test_zc6_monitor_decode.c — ZC6 CAN 监听帧载荷解码单测
//
//  覆盖:挡位 0x141 编码表(含 R 与无效码)、G 力 0x0D0 的符号与倍率、
//  胎压 0x6E2 的单位自动判定与换算、防御式 NULL 检查。
//  帧格式依据:ft86 gen1 逆向文档(ref 仓库同源实现)。
// ================================================================

#include "test_util.h"
#include "app_obd_dsp/zc6_monitor_decode.h"

int main(void)
{
    // ---- 挡位 0x141:payload[6] & 0x0F ----
    int8_t gear = 99;
    uint8_t p[8] = {0};

    p[6] = 0x00;
    TEST_ASSERT(zc6_gear_decode_141_payload(p, &gear));
    TEST_ASSERT_EQ_INT(0, gear);        // N
    p[6] = 0x03;
    TEST_ASSERT(zc6_gear_decode_141_payload(p, &gear));
    TEST_ASSERT_EQ_INT(3, gear);        // 3 挡
    p[6] = 0x06;
    TEST_ASSERT(zc6_gear_decode_141_payload(p, &gear));
    TEST_ASSERT_EQ_INT(6, gear);        // 6 挡
    p[6] = 0x07;
    TEST_ASSERT(zc6_gear_decode_141_payload(p, &gear));
    TEST_ASSERT_EQ_INT(-1, gear);       // R(obd_data_cache 挡位表示)
    p[6] = 0xF3;                        // 高 nibble 是其他信号
    TEST_ASSERT(zc6_gear_decode_141_payload(p, &gear));
    TEST_ASSERT_EQ_INT(3, gear);
    p[6] = 0x08;
    TEST_ASSERT(!zc6_gear_decode_141_payload(p, &gear));   // 8..15 无效
    TEST_ASSERT(!zc6_gear_decode_141_payload(NULL, &gear));
    TEST_ASSERT(!zc6_gear_decode_141_payload(p, NULL));

    // ---- G 力 0x0D0:lat = int8(b6)×0.2g,lon = int8(b7)×-0.1g(输出 0.01g) ----
    int16_t lat = 0, lon = 0;
    p[6] = 0x05; p[7] = 0x05;
    TEST_ASSERT(zc6_gforce_decode_0d0_payload(p, &lat, &lon));
    TEST_ASSERT_EQ_INT(100, lat);       // +5 × 20 = +1.00 g
    TEST_ASSERT_EQ_INT(-50, lon);       // +5 × -10 = -0.50 g
    p[6] = 0xFB; p[7] = 0xFB;           // -5
    TEST_ASSERT(zc6_gforce_decode_0d0_payload(p, &lat, &lon));
    TEST_ASSERT_EQ_INT(-100, lat);      // -1.00 g
    TEST_ASSERT_EQ_INT(50, lon);        // +0.50 g(负原始值 × 负倍率)
    TEST_ASSERT(!zc6_gforce_decode_0d0_payload(NULL, &lat, &lon));

    // ---- 胎压 0x6E2:单位自动判定 ----
    // 四轮原值 34(典型冷胎 ~2.35 bar):PSI 解释 2.34 bar 得分最近,BAR 解释 3.4、KPA 1.7
    uint8_t t[8] = {0};
    t[3] = 34 << 1; t[4] = 34 << 1; t[5] = 34 << 1; t[6] = 34 << 1;
    TEST_ASSERT_EQ_INT(ZC6_TPMS_SCALE_PSI, zc6_tpms_auto_resolve_scale(t));
    // 原值 23:BAR 解释 2.3 bar 得分最近(PSI 1.59 出界? 1.59 在界内但得分远;KPA 1.15 出界)
    t[3] = 23 << 1; t[4] = 23 << 1; t[5] = 23 << 1; t[6] = 23 << 1;
    TEST_ASSERT_EQ_INT(ZC6_TPMS_SCALE_BAR, zc6_tpms_auto_resolve_scale(t));
    // 原值 47:KPA 解释 2.35 bar 完美命中(PSI 3.24 在界内但得分远;BAR 4.7 出界)
    t[3] = 47 << 1; t[4] = 47 << 1; t[5] = 47 << 1; t[6] = 47 << 1;
    TEST_ASSERT_EQ_INT(ZC6_TPMS_SCALE_KPA, zc6_tpms_auto_resolve_scale(t));
    // 全零载荷(未发包):判定失败
    memset(t, 0, sizeof(t));
    TEST_ASSERT_EQ_INT(ZC6_TPMS_SCALE_AUTO, zc6_tpms_auto_resolve_scale(t));

    // ---- 胎压换算:原值 → 0.1 bar ----
    TEST_ASSERT_EQ_INT(23, zc6_tpms_pressure_bar_x10(34, ZC6_TPMS_SCALE_PSI));   // 2.34 bar
    TEST_ASSERT_EQ_INT(23, zc6_tpms_pressure_bar_x10(23, ZC6_TPMS_SCALE_BAR));  // 2.30 bar
    TEST_ASSERT_EQ_INT(24, zc6_tpms_pressure_bar_x10(47, ZC6_TPMS_SCALE_KPA));  // 2.35 bar → 23.5 取整
    TEST_ASSERT_EQ_INT(-1, zc6_tpms_pressure_bar_x10(34, ZC6_TPMS_SCALE_AUTO));

    // ---- 胎压整帧解码:0 原值轮写 -1(无效) ----
    t[3] = 34 << 1; t[4] = 0; t[5] = 34 << 1; t[6] = 34 << 1;
    int16_t bar[4] = {0};
    TEST_ASSERT(zc6_tpms_decode_6e2_payload(t, ZC6_TPMS_SCALE_PSI, bar));
    TEST_ASSERT_EQ_INT(23, bar[0]);
    TEST_ASSERT_EQ_INT(-1, bar[1]);     // 该轮未发包
    TEST_ASSERT_EQ_INT(23, bar[2]);
    TEST_ASSERT_EQ_INT(23, bar[3]);
    TEST_ASSERT(!zc6_tpms_decode_6e2_payload(t, ZC6_TPMS_SCALE_AUTO, bar));      // 单位未判定
    TEST_ASSERT(!zc6_tpms_decode_6e2_payload(NULL, ZC6_TPMS_SCALE_PSI, bar));

    return TEST_RESULT();
}
