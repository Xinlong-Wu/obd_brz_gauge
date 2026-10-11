// ================================================================
//  test_jdm_widgets.c — readout 数值格式化纯逻辑单测
//
//  覆盖 jdm_readout_logic.h 全部分支:各通道除数约定、负值、
//  无效哨兵文本、缓冲防御。
// ================================================================

#include "test_util.h"
#include "widgets/jdm_readout_logic.h"

int main(void)
{
    char buf[16];

    // ---- 无效 → em dash(设计断连语义) ----
    TEST_ASSERT_EQ_STR("\xE2\x80\x94", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_RPM, 0, false));

    // ---- 整数通道 ----
    TEST_ASSERT_EQ_STR("4800", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_RPM, 4800, true));
    TEST_ASSERT_EQ_STR("92", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_CLT, 92, true));
    TEST_ASSERT_EQ_STR("-10", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_CLT, -10, true));
    TEST_ASSERT_EQ_STR("105", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_SPEED, 105, true));

    // ---- BAT:mV → 1 位小数(截断) ----
    TEST_ASSERT_EQ_STR("14.2", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BAT, 14200, true));
    TEST_ASSERT_EQ_STR("12.0", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BAT, 12000, true));

    // ---- OILP / TPMS:0.1 bar → 1 位小数 ----
    TEST_ASSERT_EQ_STR("3.8", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_OILP, 38, true));
    TEST_ASSERT_EQ_STR("2.2", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_TPMS_FL, 22, true));

    // ---- BKT:0.1°C → 整数 ----
    TEST_ASSERT_EQ_STR("600", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BKT, 6000, true));

    // ---- BOOST:可负 1 位小数 ----
    TEST_ASSERT_EQ_STR("0.60", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, 6, true));
    TEST_ASSERT_EQ_STR("-0.60", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, -6, true));
    TEST_ASSERT_EQ_STR("1.20", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_BOOST, 12, true));

    // ---- AFR:×100 → 1 位小数 ----
    TEST_ASSERT_EQ_STR("14.7", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_AFR, 1470, true));

    // ---- G 力:×100 → 2 位小数 ----
    TEST_ASSERT_EQ_STR("0.52", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_GFORCE_LAT, 52, true));
    TEST_ASSERT_EQ_STR("-1.05", jdm_readout_logic_format(buf, sizeof(buf), DISP_ITEM_GFORCE_LON, -105, true));

    // ---- 缓冲防御:长度 0 不写 ----
    char tiny[4] = {0};
    TEST_ASSERT(!jdm_readout_logic_format(NULL, sizeof(tiny), DISP_ITEM_RPM, 1, true));
    jdm_readout_logic_format(tiny, 0, DISP_ITEM_RPM, 1, true);

    if (t_failed) {
        fprintf(stderr, "%d/%d checks failed\n", t_failed, t_checks);
        return 1;
    }
    printf("test_jdm_widgets: %d checks passed\n", t_checks);
    return 0;
}
