// ================================================================
//  test_obd_data_cache.c — 数据缓存单测
//
//  覆盖:初始无效哨兵、各通道 set/get 往返、RPM 覆盖层(三连表联动
//  测试用)、油温无效化、快照一致性、calculate_gear 的挡位区间选择。
// ================================================================

#include "test_util.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"

int main(void)
{
    // ---- 初始哨兵:进程刚起、未写过任何值 ----
    TEST_ASSERT_EQ_INT(-40, obd_data_get_coolant_temp());
    TEST_ASSERT_EQ_INT(-100, obd_data_get_oil_temp());
    TEST_ASSERT_EQ_INT(-1, obd_data_get_load_pct());
    TEST_ASSERT_EQ_INT(-1, obd_data_get_tps());
    TEST_ASSERT_EQ_INT(-1, obd_data_get_bat_mv());
    TEST_ASSERT_EQ_INT(-1, obd_data_get_oil_pressure_x10());
    TEST_ASSERT_EQ_INT(-32768, obd_data_get_boost_x10());
    TEST_ASSERT_EQ_INT(-1000, obd_data_get_brake_temp_x10());
    TEST_ASSERT_EQ_INT(-1, obd_data_get_afr_x100());

    // ---- set/get 往返(每通道一个典型值) ----
    obd_data_set_rpm(3456);
    TEST_ASSERT_EQ_INT(3456, obd_data_get_rpm());
    obd_data_set_speed(87);
    TEST_ASSERT_EQ_INT(87, obd_data_get_speed());
    obd_data_set_coolant_temp(92);
    TEST_ASSERT_EQ_INT(92, obd_data_get_coolant_temp());
    obd_data_set_oil_temp(104);
    TEST_ASSERT_EQ_INT(104, obd_data_get_oil_temp());
    obd_data_set_intake_temp(38);
    TEST_ASSERT_EQ_INT(38, obd_data_get_intake_temp());
    obd_data_set_load_pct(42);
    TEST_ASSERT_EQ_INT(42, obd_data_get_load_pct());
    obd_data_set_tps(63);
    TEST_ASSERT_EQ_INT(63, obd_data_get_tps());
    obd_data_set_bat_mv(13900);
    TEST_ASSERT_EQ_INT(13900, obd_data_get_bat_mv());
    obd_data_set_oil_pressure_x10(35);
    TEST_ASSERT_EQ_INT(35, obd_data_get_oil_pressure_x10());
    obd_data_set_boost_x10(123);
    TEST_ASSERT_EQ_INT(123, obd_data_get_boost_x10());
    obd_data_set_brake_temp_x10(3210);
    TEST_ASSERT_EQ_INT(3210, obd_data_get_brake_temp_x10());
    obd_data_set_afr_x100(1470);
    TEST_ASSERT_EQ_INT(1470, obd_data_get_afr_x100());

    // ---- 油温无效化(车型不支持油温路径时) ----
    obd_data_set_oil_temp_invalid();
    TEST_ASSERT_EQ_INT(-100, obd_data_get_oil_temp());

    // ---- 快照:与单通道 getter 一致 ----
    obd_data_snapshot_t snap;
    obd_data_get_snapshot(&snap);
    TEST_ASSERT_EQ_INT(3456, snap.rpm);
    TEST_ASSERT_EQ_INT(87, snap.speed);
    TEST_ASSERT_EQ_INT(92, snap.coolant_temp);
    TEST_ASSERT_EQ_INT(-100, snap.oil_temp);
    TEST_ASSERT_EQ_INT(38, snap.intake_temp);
    TEST_ASSERT_EQ_INT(13900, snap.bat_mv);
    TEST_ASSERT_EQ_INT(123, snap.boost_x10);
    TEST_ASSERT_EQ_INT(1470, snap.afr_x100);
    TEST_ASSERT_EQ_INT(BRAKE_RS485_IDLE, snap.brake_rs485_status);

    obd_data_set_brake_rs485_status(BRAKE_RS485_OK);
    obd_data_get_snapshot(&snap);
    TEST_ASSERT_EQ_INT(BRAKE_RS485_OK, snap.brake_rs485_status);

    // ---- RPM 覆盖层:开启时 get 返回覆盖值,关闭后恢复真实值 ----
    obd_data_rpm_override_set(true, 7000);
    TEST_ASSERT_EQ_INT(7000, obd_data_get_rpm());
    obd_data_rpm_override_set(false, 0);
    TEST_ASSERT_EQ_INT(3456, obd_data_get_rpm());

    // ---- 挡位:直接写读(含 R / 无效哨兵 127) ----
    obd_data_set_gear(3);
    TEST_ASSERT_EQ_INT(3, obd_data_get_gear());
    obd_data_set_gear(127);
    TEST_ASSERT_EQ_INT(127, obd_data_get_gear());

    // ---- calculate_gear:默认车型(OBD2 Generic,6MT 占位速比) ----
    // 零速/零转 → N
    TEST_ASSERT_EQ_INT(GEAR_NEUTRAL, calculate_gear(0, 50));
    TEST_ASSERT_EQ_INT(GEAR_NEUTRAL, calculate_gear(3000, 0));

    // 构造总传动比精确落进某挡区间:total = rpm / (speed * calc_const),
    // 区间中心 = gear_ratio × final_drive(rebuild_gear_ranges 的约定)
    vehicle_profile_set_active(0);
    const vehicle_profile_t *prof = vehicle_profile_get_active();
    TEST_ASSERT(prof != NULL);
    float calc_const = vehicle_profile_calc_constant(prof);
    TEST_ASSERT(calc_const > 0.0f);

    // 1 挡速比 3.5、4 挡速比 1.1(占位表 {0,3.5,2.0,1.4,1.1,0.9,0.75}),尾牙 3.5
    float rpm1 = prof->gear_ratios[1] * prof->final_drive_ratio * calc_const * 40.0f;
    TEST_ASSERT_EQ_INT(GEAR_1, calculate_gear(rpm1, 40));
    float rpm4 = prof->gear_ratios[4] * prof->final_drive_ratio * calc_const * 80.0f;
    TEST_ASSERT_EQ_INT(GEAR_4, calculate_gear(rpm4, 80));

    return TEST_RESULT();
}
