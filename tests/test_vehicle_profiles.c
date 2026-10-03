// ================================================================
//  test_vehicle_profiles.c — 车型配置表单测
//
//  覆盖:表数量与 docs/VEHICLES.md 一致、索引越界钳制、激活态切换、
//  每条 profile 的数据自洽(速比/挡位数/油温策略)。
//  红线关联:表内容变化必须同步 VEHICLES.md(AGENTS.md 映射表)。
// ================================================================

#include "test_util.h"
#include "app_obd_dsp/vehicle_profiles.h"

int main(void)
{
    // ---- 表规模 ----
    uint8_t count = 0;
    const vehicle_profile_t *all = vehicle_profile_get_all(&count);
    TEST_ASSERT(all != NULL);
    TEST_ASSERT(count > 0);
    // 数量与 VEHICLES.md/README 的车型清单一致:改表请同步文档
    TEST_ASSERT_EQ_INT(17, count);

    // ---- 索引访问:界内返回条目,越界安全 ----
    TEST_ASSERT(vehicle_profile_get(0) != NULL);
    TEST_ASSERT_EQ_STR("OBD2 Generic", vehicle_profile_get(0)->name);
    TEST_ASSERT(vehicle_profile_get(count - 1) != NULL);

    // ---- 激活态:set_active 越界钳制到 0(见 vehicle_profiles.c) ----
    vehicle_profile_set_active(count + 5);
    TEST_ASSERT_EQ_STR("OBD2 Generic", vehicle_profile_get_active()->name);

    vehicle_profile_set_active(2);
    TEST_ASSERT_EQ_STR(vehicle_profile_get(2)->name, vehicle_profile_get_active()->name);

    // ---- 每条 profile 的数据自洽 ----
    for (uint8_t i = 0; i < count; i++) {
        const vehicle_profile_t *p = vehicle_profile_get(i);
        TEST_ASSERT(p->name != NULL && p->name[0] != '\0');
        // gear_count: 0 = CVT 无离散挡(禁用推算),上限 8(ZF 8HP)
        TEST_ASSERT(p->gear_count <= 8);
        TEST_ASSERT(p->final_drive_ratio > 0.0f);
        TEST_ASSERT(p->tire_rolling_radius_m > 0.1f && p->tire_rolling_radius_m < 1.0f);
        // 1..gear_count 挡速比必须为正且单调递减(CVT 的 gear_count=0 自动跳过)
        for (uint8_t g = 1; g <= p->gear_count; g++) {
            TEST_ASSERT(p->gear_ratios[g] > 0.0f);
            if (g > 1) {
                TEST_ASSERT(p->gear_ratios[g] < p->gear_ratios[g - 1]);
            }
        }
        // 油温策略:NONE(0xFF)合法(该车型无油温路径),其余必须在枚举界内
        TEST_ASSERT(p->oil_temp_strategy.primary == OIL_TEMP_MODE_NONE ||
                    p->oil_temp_strategy.primary <= OIL_TEMP_MODE_BMW_22_111F);
    }

    // ---- 挡位区间表:由速比 + 容差生成,数量与挡位吻合 ----
    vehicle_profile_set_active(0);
    uint8_t range_count = 0;
    const gear_ratio_range_t *ranges = vehicle_profile_get_gear_ranges(&range_count);
    TEST_ASSERT(ranges != NULL);
    TEST_ASSERT_EQ_INT(vehicle_profile_get_active()->gear_count, range_count);
    for (uint8_t r = 0; r < range_count; r++) {
        TEST_ASSERT(ranges[r].min_ratio <= ranges[r].max_ratio);
        TEST_ASSERT(ranges[r].gear >= GEAR_1 && ranges[r].gear <= GEAR_8);
    }

    // calc_constant(速度→轮速换算常数)必须为正
    TEST_ASSERT(vehicle_profile_calc_constant(vehicle_profile_get_active()) > 0.0f);

    return TEST_RESULT();
}
