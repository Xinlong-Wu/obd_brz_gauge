// ================================================================
//  test_jdm_data.c — JDM 统一数据通道单测
//
//  覆盖:通道哨兵判定(有效/无效/越界)、元数据与量程表、
//  jdm_data_logic.h 纯逻辑(pct/弧角/转速段/温度柱)。
// ================================================================

#include "test_util.h"
#include "core/jdm_data.h"
#include "core/jdm_data_logic.h"

#include "app_obd_dsp/obd_data_cache.h"

int main(void)
{
    // ---- 越界防御 ----
    int32_t v = 0;
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_COUNT, &v));
    TEST_ASSERT(!jdm_data_read_cache((disp_item_t)-1, &v));
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_RPM, NULL));

    // ---- 恒有效通道:RPM/SPEED 直接放行 ----
    obd_data_set_rpm(4800);
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_RPM, &v) && v == 4800);
    obd_data_set_speed(105);
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_SPEED, &v) && v == 105);

    // ---- 哨兵判定:与 obd_data_cache 约定一致 ----
    obd_data_set_coolant_temp(-100);          // <= -40 无效
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_CLT, &v));
    obd_data_set_coolant_temp(92);
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_CLT, &v) && v == 92);

    obd_data_set_oil_temp(-100);              // 油温哨兵 -100
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_OIL, &v));
    obd_data_set_oil_temp(104);
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_OIL, &v) && v == 104);

    obd_data_set_boost_x10(-32768);           // boost 哨兵
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_BOOST, &v));
    obd_data_set_boost_x10(68);               // 0.68 bar(raw 0.1bar)
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_BOOST, &v) && v == 68);

    obd_data_set_tpms_bar_x10(0, -1);         // 胎压无效
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_TPMS_FL, &v));
    obd_data_set_tpms_bar_x10(0, 22);
    TEST_ASSERT(jdm_data_read_cache(DISP_ITEM_TPMS_FL, &v) && v == 22);

    obd_data_set_gforce_x100(-32768, 0);
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_GFORCE_LAT, &v));

    // ---- 心率:数据源接入前恒无效 ----
    TEST_ASSERT(!jdm_data_read_cache(DISP_ITEM_HEART, &v));

    // ---- 元数据/量程表:越界回退第 0 项 ----
    TEST_ASSERT(jdm_data_meta(DISP_ITEM_OIL)->unit[0] == '\'');
    TEST_ASSERT(jdm_data_meta(DISP_ITEM_COUNT) == jdm_data_meta(DISP_ITEM_CLT));
    TEST_ASSERT(jdm_data_range(DISP_ITEM_RPM)->nmax == 8000);
    TEST_ASSERT(jdm_data_range(DISP_ITEM_COUNT) == jdm_data_range(DISP_ITEM_CLT));

    // ---- 纯逻辑:pct(含 div 折算与钳位) ----
    const jdm_data_range_t *rr = jdm_data_range(DISP_ITEM_RPM);
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_pct(0, rr));
    TEST_ASSERT_EQ_INT(50, jdm_data_logic_pct(4000, rr));
    TEST_ASSERT_EQ_INT(100, jdm_data_logic_pct(8000, rr));
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_pct(-500, rr));    // 负值钳 0
    TEST_ASSERT_EQ_INT(100, jdm_data_logic_pct(9999, rr));  // 超量程钳 100

    const jdm_data_range_t *br = jdm_data_range(DISP_ITEM_BOOST);
    // BOOST 量程 -10..30 (0.1bar),raw=68 → 0.68bar... wait: raw/div = 68/10 = 6 (整数除法截断)
    // 语义按整数除法走:与旧 needle 换算一致,测文档化行为
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_pct(-100, br));    // raw -10.0bar 钳 0

    // ---- 纯逻辑:270° 弧终点角 ----
    TEST_ASSERT_EQ_INT(135, jdm_data_logic_arc270_end(0));
    TEST_ASSERT_EQ_INT(270, jdm_data_logic_arc270_end(50));
    TEST_ASSERT_EQ_INT(405, jdm_data_logic_arc270_end(100));

    // ---- 纯逻辑:56 段转速点亮数(ceil;量程为车型配置参数) ----
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_tach_lit(0, 8000));
    TEST_ASSERT_EQ_INT(1, jdm_data_logic_tach_lit(1, 8000));      // >0 保底 1 段
    TEST_ASSERT_EQ_INT(34, jdm_data_logic_tach_lit(4800, 8000));  // ceil(33.6)
    TEST_ASSERT_EQ_INT(56, jdm_data_logic_tach_lit(8000, 8000));
    TEST_ASSERT_EQ_INT(56, jdm_data_logic_tach_lit(9000, 8000));  // 超转钳满
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_tach_lit(-100, 8000));
    TEST_ASSERT_EQ_INT(56, jdm_data_logic_tach_lit(6000, 6000));  // 车型量程=6000

    // ---- 纯逻辑:段角度几何(140° 起,扫 260°;浮点精确节距 4.6429°) ----
    TEST_ASSERT_NEAR(140.0, jdm_data_logic_tach_segment_start(0), 0.01);
    TEST_ASSERT_NEAR(167.857, jdm_data_logic_tach_segment_start(6), 0.01);
    TEST_ASSERT_NEAR(395.357, jdm_data_logic_tach_segment_start(55), 0.01);
    for (int i = 1; i < 56; i++) {
        TEST_ASSERT(jdm_data_logic_tach_segment_start(i) >= jdm_data_logic_tach_segment_start(i - 1));
        TEST_ASSERT_NEAR(4.6429, jdm_data_logic_tach_segment_start(i) - jdm_data_logic_tach_segment_start(i - 1), 0.01);
    }

    // ---- 纯逻辑:段色带(段起始转速 vs 车型警戒区;量程为参数) ----
    TEST_ASSERT_EQ_INT(5571, jdm_data_logic_tach_seg_rpm(39, 8000));  // < 5700 → 白
    TEST_ASSERT_EQ_INT(5714, jdm_data_logic_tach_seg_rpm(40, 8000));  // ≥ 5700 → 琥珀
    TEST_ASSERT_EQ_INT(6857, jdm_data_logic_tach_seg_rpm(48, 8000));  // < 7000 → 琥珀
    TEST_ASSERT_EQ_INT(7000, jdm_data_logic_tach_seg_rpm(49, 8000));  // ≥ 7000 → 红(=RPM critical)
    TEST_ASSERT_EQ_INT(7857, jdm_data_logic_tach_seg_rpm(55, 8000));
    TEST_ASSERT_EQ_INT(5357, jdm_data_logic_tach_seg_rpm(50, 6000));  // 车型量程=6000

    // ---- 纯逻辑:温度柱 ‰ ----
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_thermal_permille(50));
    TEST_ASSERT_EQ_INT(0, jdm_data_logic_thermal_permille(60));
    TEST_ASSERT_EQ_INT(500, jdm_data_logic_thermal_permille(90));
    TEST_ASSERT_EQ_INT(1000, jdm_data_logic_thermal_permille(120));
    TEST_ASSERT_EQ_INT(1000, jdm_data_logic_thermal_permille(130));

    if (t_failed) {
        fprintf(stderr, "%d/%d checks failed\n", t_failed, t_checks);
        return 1;
    }
    printf("test_jdm_data: %d checks passed\n", t_checks);
    return 0;
}
