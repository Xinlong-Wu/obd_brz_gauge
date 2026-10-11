#pragma once
// ================================================================
//  jdm_conn.h — 页面级"车况在线"判定
//
//  主表/独立表 = ELM327 已连接;从表 = 已收到主表 ESP-NOW 数据。
//  断连时页面显示 "—" / "NO SIGNAL"(原型 disconnected 语义)。
// ================================================================

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 当前设备是否在线(任一数据源活着)。 */
bool jdm_conn_active(void);

#ifdef __cplusplus
}
#endif
