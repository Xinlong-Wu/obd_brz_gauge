#pragma once
// ================================================================
//  jdm_data.h — JDM 统一数据通道
//
//  词汇表 disp_item_t 的枚举值是持久化契约(NVS dashboard 槽项、
//  ESP-NOW 无关),0..17 与旧 ui_disp_item 完全一致,新通道只能追加
//  到末尾。读取一律走 jdm_data_read_cache():内部直读 obd_data_cache
//  (portMUX 临界区,BLE/ESP-NOW/RS485 任务写、LVGL 任务读,天然安全),
//  并按通道哨兵规则判定有效性——无效即"断连/无传感器",组件显示 "—"。
// ================================================================

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DISP_ITEM_CLT = 0,
    DISP_ITEM_IAT,
    DISP_ITEM_OIL,
    DISP_ITEM_LOAD,
    DISP_ITEM_TPS,
    DISP_ITEM_RPM,
    DISP_ITEM_SPEED,
    DISP_ITEM_BAT,
    DISP_ITEM_OILP,
    DISP_ITEM_BKT,
    DISP_ITEM_BOOST,
    DISP_ITEM_AFR,
    // ---- 追加通道:枚举值只能追加,历史值不可改 ----
    DISP_ITEM_TPMS_FL,     // 胎压 0.1 bar (ZC6 0x6E2)
    DISP_ITEM_TPMS_FR,
    DISP_ITEM_TPMS_RL,
    DISP_ITEM_TPMS_RR,
    DISP_ITEM_GFORCE_LAT,  // 0.01 g (ZC6 0x0D0 / IMU)
    DISP_ITEM_GFORCE_LON,
    DISP_ITEM_HEART,       // 心率 bpm(BLE HR 0x180D;数据源接入前恒无效)
    DISP_ITEM_COUNT,
} disp_item_t;

typedef struct {
    const char *name;   // 短名(通道标识,如 "CLT")
    const char *unit;   // 单位(如 "'C";空串 = 无单位)
} jdm_data_meta_t;

/** 自然量程:nmin/nmax 为自然单位,div 满足 raw = natural × div
 *  (BAT: mV→V;OILP/BKT/TPMS: 0.1 单位;GFORCE/AFR: 0.01)。 */
typedef struct {
    int32_t nmin;
    int32_t nmax;
    int32_t div;
} jdm_data_range_t;

/** 读通道原始值。返回 false = 无效(断连/无传感器/越界哨兵),
 *  *out 不写。 */
bool jdm_data_read_cache(disp_item_t item, int32_t *out);

/** 通道元数据(名称/单位);越界返回第 0 项。 */
const jdm_data_meta_t *jdm_data_meta(disp_item_t item);

/** 通道自然量程;越界返回第 0 项。 */
const jdm_data_range_t *jdm_data_range(disp_item_t item);

#ifdef __cplusplus
}
#endif
