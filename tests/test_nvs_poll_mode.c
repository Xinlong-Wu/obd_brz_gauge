// ================================================================
//  test_nvs_poll_mode.c — OBD 轮询档位映射单测
//
//  覆盖:三档间隔取值、越界回退 NORMAL(与 nvs_storage.c 修复块、
//  elm327_ble_client.c 间隔解析链的约定一致)。
// ================================================================

#include "test_util.h"
#include "bsp_obd_dsp/nvs_storage.h"

int main(void)
{
    // 三档间隔(NORMAL 是历史默认 30ms,兼容老设备 NVS 的 grow 零值)
    TEST_ASSERT_EQ_INT(30, nvs_obd_poll_mode_default_gap_ms(NVS_OBD_POLL_MODE_NORMAL));
    TEST_ASSERT_EQ_INT(15, nvs_obd_poll_mode_default_gap_ms(NVS_OBD_POLL_MODE_FAST));
    TEST_ASSERT_EQ_INT(5,  nvs_obd_poll_mode_default_gap_ms(NVS_OBD_POLL_MODE_TURBO));

    // 越界/损坏值回退 NORMAL(nvs_storage.c 修复块同款语义)
    TEST_ASSERT_EQ_INT(30, nvs_obd_poll_mode_default_gap_ms(NVS_OBD_POLL_MODE_COUNT));
    TEST_ASSERT_EQ_INT(30, nvs_obd_poll_mode_default_gap_ms(200));

    // mock 的默认配置:老设备/新装都落在 NORMAL
    TEST_ASSERT_EQ_INT(NVS_OBD_POLL_MODE_NORMAL, nvs_cfg_get()->obd_poll_mode);

    // 写读往返
    nvs_user_cfg_t cfg = *nvs_cfg_get();
    cfg.obd_poll_mode = NVS_OBD_POLL_MODE_TURBO;
    nvs_cfg_set(&cfg);
    TEST_ASSERT_EQ_INT(NVS_OBD_POLL_MODE_TURBO, nvs_cfg_get()->obd_poll_mode);

    return TEST_RESULT();
}
