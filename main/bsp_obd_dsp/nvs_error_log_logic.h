#pragma once
// ================================================================
//  nvs_error_log_logic.h — 错误日志环形缓冲纯逻辑(static inline)
//
//  sanitize(载入后修复损坏的版本/游标)与 append(写一条、回绕、计数
//  饱和)不碰 NVS 与锁,由 nvs_storage.c 持锁调用;单测直接编译断言
//  (tests/test_nvs_error_log.c)。移植自 Hokori23/obd_brz_gauge。
// ================================================================

#include <stdio.h>
#include <string.h>

#include "bsp_obd_dsp/nvs_storage.h"

/** 修复载入后的损坏状态:版本不匹配整体清零重来;head/count 越界钳回。 */
static inline void nvs_error_log_logic_sanitize(nvs_error_log_t *log)
{
    if (log == NULL) return;

    if (log->version != NVS_ERROR_LOG_VERSION) {
        memset(log, 0, sizeof(*log));
        log->version = NVS_ERROR_LOG_VERSION;
        return;
    }

    if (log->head >= NVS_ERROR_LOG_CAPACITY) log->head = 0;
    if (log->count > NVS_ERROR_LOG_CAPACITY) log->count = NVS_ERROR_LOG_CAPACITY;
}

/** 追加一条(seq 单调;head 回绕;count 饱和于容量)。tag/message 可为 NULL。 */
static inline void nvs_error_log_logic_append(nvs_error_log_t *log,
                                              uint32_t uptime_s,
                                              int32_t err_code,
                                              const char *tag,
                                              const char *message)
{
    nvs_error_entry_t *entry;

    if (log == NULL) return;

    nvs_error_log_logic_sanitize(log);

    entry = &log->entries[log->head];
    memset(entry, 0, sizeof(*entry));
    entry->seq = log->next_seq++;
    entry->uptime_s = uptime_s;
    entry->err_code = err_code;

    if (tag != NULL)     snprintf(entry->tag, sizeof(entry->tag), "%s", tag);
    if (message != NULL) snprintf(entry->message, sizeof(entry->message), "%s", message);

    log->head = (uint8_t)((log->head + 1u) % NVS_ERROR_LOG_CAPACITY);
    if (log->count < NVS_ERROR_LOG_CAPACITY) log->count++;
}
