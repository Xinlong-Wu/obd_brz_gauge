// ================================================================
//  jdm_conn.c — 在线判定实现
// ================================================================

#include "jdm_conn.h"

#include "bsp_obd_dsp/elm327_ble_client.h"
#include "bsp_obd_dsp/espnow_link.h"
#include "bsp_obd_dsp/nvs_storage.h"

bool jdm_conn_active(void)
{
    if (elm327_ble_is_connected()) return true;
    // 从表:绑定了主表且已收到广播数据
    if (nvs_cfg_get()->device_role == ESPNOW_ROLE_SLAVE) {
        return espnow_link_slave_has_data();
    }
    return false;
}
