// ================================================================
//  test_jdm_alerts.c — 通道报警阈值纯逻辑边界
// ================================================================

#include "test_util.h"

#include "core/jdm_alerts_logic.h"

int main(void)
{
    const char *r = NULL;

    // ---- 转速:advisory(红区前) / critical(红区起点,进红区即报) ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_rpm(6799, 6800, 7000, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_ADVISORY, jdm_alerts_logic_rpm(6800, 6800, 7000, &r));
    TEST_ASSERT_EQ_STR("SHIFT", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_ADVISORY, jdm_alerts_logic_rpm(6999, 6800, 7000, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, jdm_alerts_logic_rpm(7000, 6800, 7000, &r));
    TEST_ASSERT_EQ_STR("RPM LIMIT", r);
    // 车型可配:换挡/红区随配置平移(例:9000 转高转车型)
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_rpm(8499, 8500, 9000, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, jdm_alerts_logic_rpm(9000, 8500, 9000, &r));

    // ---- 油温/水温:≥110 warning ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_oil(109, 110, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_oil(130, 130, &r));
    TEST_ASSERT_EQ_STR("OIL TEMP HIGH", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_clt(109, 110, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_clt(112, 110, &r));
    TEST_ASSERT_EQ_STR("COOLANT HIGH", r);

    // ---- 油压:≤阈值 critical(默认 10 = 1.0 bar 不告警) ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, jdm_alerts_logic_oilp(5, 10, &r));
    TEST_ASSERT_EQ_STR("OIL PRESS LOW", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_oilp(10, 10, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, jdm_alerts_logic_oilp(15, 20, &r));   // 车型可配

    // ---- 增压:阈值 = 车型红区起点(默认 2.4 bar) ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_boost(23, 24, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_boost(24, 24, &r));
    TEST_ASSERT_EQ_STR("BOOST HIGH", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_boost(19, 20, &r));   // 车型可配
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_boost(20, 20, &r));

    // ---- 电压:≤10.8 / ≥16.4 V ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_bat(10800, &r));
    TEST_ASSERT_EQ_STR("VOLTAGE LOW", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_bat(10801, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_bat(16399, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_bat(16400, &r));
    TEST_ASSERT_EQ_STR("VOLTAGE HIGH", r);

    // ---- 胎压:≤0.5 critical / ≥3.4 warning ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, jdm_alerts_logic_tyre(5));
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_tyre(6));
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_tyre(33));
    TEST_ASSERT_EQ_INT(JDM_ALERT_WARNING, jdm_alerts_logic_tyre(34));

    // ---- G 值:任一轴 >1.0g advisory ----
    TEST_ASSERT_EQ_INT(JDM_ALERT_NONE, jdm_alerts_logic_gforce(100, -100, &r));
    TEST_ASSERT_EQ_INT(JDM_ALERT_ADVISORY, jdm_alerts_logic_gforce(101, 0, &r));
    TEST_ASSERT_EQ_STR("G LIMIT", r);
    TEST_ASSERT_EQ_INT(JDM_ALERT_ADVISORY, jdm_alerts_logic_gforce(0, -190, &r));

    // ---- push:critical 优先、同级保序、满员/NONE 安全 ----
    jdm_alert_item_t arr[3];
    int n = 0;
    n = jdm_alerts_logic_push(arr, n, 3, "BOOST HIGH", JDM_ALERT_WARNING);
    n = jdm_alerts_logic_push(arr, n, 3, "OIL PRESS LOW", JDM_ALERT_CRITICAL);
    n = jdm_alerts_logic_push(arr, n, 3, "OIL TEMP HIGH", JDM_ALERT_WARNING);
    TEST_ASSERT_EQ_INT(3, n);
    TEST_ASSERT_EQ_STR("OIL PRESS LOW", arr[0].reason);
    TEST_ASSERT_EQ_INT(JDM_ALERT_CRITICAL, arr[0].level);
    TEST_ASSERT_EQ_STR("BOOST HIGH", arr[1].reason);
    TEST_ASSERT_EQ_STR("OIL TEMP HIGH", arr[2].reason);
    n = jdm_alerts_logic_push(arr, n, 3, "FULL", JDM_ALERT_ADVISORY);
    TEST_ASSERT_EQ_INT(3, n);
    n = jdm_alerts_logic_push(arr, n, 3, "NOPE", JDM_ALERT_NONE);
    TEST_ASSERT_EQ_INT(3, n);

    return TEST_RESULT();
}
