#pragma once
// ================================================================
//  ui_dashboard_config.h — 仪表页滚轮配置页(M4.d)
//
//  长按 EDIT 进入:TYPE(METRIC/GEAR/GFORCE)→ METRIC 时 SLOTS(1..6)→
//  每槽 CHANNEL(统一通道词汇表,18 项滚轮);GEAR/GFORCE 无槽位。
//  每次变更 nvs_dashboard_page_set 持久化,退出时由 home 重建生效。
// ================================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 打开配置页(编辑 dashboard.pages[page_idx],0 基);返回 home 由本页手势负责。 */
void ui_dashboard_config_open(uint8_t page_idx);

#ifdef __cplusplus
}
#endif
