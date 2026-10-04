// ================================================================
//  test_nvs_error_log.c — 错误日志环形缓冲单测
//
//  覆盖:版本不符整体重置、head/count 越界钳制、追加与回绕(seq 单调、
//  count 饱和)、NULL tag/message 容忍、mock API 的 count/copy 一致性。
//  持久化路径(NVS blob)属于固件侧,由固件构建 + 真机覆盖。
// ================================================================

#include "test_util.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/nvs_error_log_logic.h"

int main(void)
{
    // ---- sanitize:版本不符 → 整体清零并重打版本号 ----
    nvs_error_log_t log = {0};
    log.version = 99u;               // 旧/损坏版本
    log.head = 200;                  // 越界游标
    log.count = 250;
    log.next_seq = 42;
    nvs_error_log_logic_sanitize(&log);
    TEST_ASSERT_EQ_INT(NVS_ERROR_LOG_VERSION, log.version);
    TEST_ASSERT_EQ_INT(0, log.head);
    TEST_ASSERT_EQ_INT(0, log.count);
    TEST_ASSERT_EQ_INT(0, log.next_seq);   // 整体清零,seq 重新开始

    // ---- sanitize:版本正确时只钳游标,不清数据 ----
    memset(&log, 0, sizeof(log));
    log.version = NVS_ERROR_LOG_VERSION;
    log.next_seq = 7;
    log.head = 70;                   // 越界 → 0
    log.count = 200;                 // 越界 → 容量
    log.entries[0].seq = 6;
    nvs_error_log_logic_sanitize(&log);
    TEST_ASSERT_EQ_INT(7, log.next_seq);
    TEST_ASSERT_EQ_INT(0, log.head);
    TEST_ASSERT_EQ_INT(NVS_ERROR_LOG_CAPACITY, log.count);
    TEST_ASSERT_EQ_INT(6, log.entries[0].seq);   // 数据保留

    nvs_error_log_logic_sanitize(NULL);   // NULL 容忍

    // ---- append:字段写入 + 回绕 + 饱和 ----
    memset(&log, 0, sizeof(log));
    log.version = NVS_ERROR_LOG_VERSION;
    for (int i = 0; i < NVS_ERROR_LOG_CAPACITY + 10; i++) {
        nvs_error_log_logic_append(&log, (uint32_t)i, -1, "elm327", "connect timeout");
    }
    TEST_ASSERT_EQ_INT(NVS_ERROR_LOG_CAPACITY, log.count);        // 饱和
    TEST_ASSERT_EQ_INT(10, log.head);                             // (0 + CAP+10) % CAP
    TEST_ASSERT_EQ_INT(NVS_ERROR_LOG_CAPACITY + 10, log.next_seq);
    // 最新一条写在 head-1(第二圈覆写了 0..9 槽)
    const nvs_error_entry_t *last = &log.entries[log.head - 1];
    TEST_ASSERT_EQ_INT(NVS_ERROR_LOG_CAPACITY + 9, last->seq);
    TEST_ASSERT_EQ_STR("elm327", last->tag);
    TEST_ASSERT_EQ_STR("connect timeout", last->message);

    // ---- append:NULL tag/message 容忍(空串),append 内部先 sanitize ----
    nvs_error_log_logic_append(&log, 1, 0x1101, NULL, NULL);
    TEST_ASSERT_EQ_STR("", log.entries[log.head - 1].tag);
    TEST_ASSERT_EQ_STR("", log.entries[log.head - 1].message);
    TEST_ASSERT_EQ_INT(0x1101, log.entries[log.head - 1].err_code);

    // 长消息截断不越界(64 字节缓冲,填 100 个字符)
    nvs_error_log_logic_append(&log, 2, 0, "t",
        "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
    TEST_ASSERT_EQ_INT(63, strnlen(log.entries[log.head - 1].message, sizeof(log.entries[0].message)));
    nvs_error_log_logic_append(NULL, 0, 0, NULL, NULL);   // NULL 容忍

    // ---- mock API:count/copy 与直写逻辑一致 ----
    TEST_ASSERT_EQ_INT(0, nvs_error_log_count());
    nvs_error_log_record("test", 0x201, "mock entry");
    nvs_error_log_record("test", 0x202, "second");
    TEST_ASSERT_EQ_INT(2, nvs_error_log_count());
    nvs_error_log_t snap;
    nvs_error_log_copy(&snap);
    TEST_ASSERT_EQ_INT(2, snap.count);
    TEST_ASSERT_EQ_STR("mock entry", snap.entries[0].message);
    TEST_ASSERT_EQ_STR("second", snap.entries[1].message);
    nvs_error_log_recordf("test", 0x203, "fmt %d ok", 7);
    nvs_error_log_copy(&snap);
    TEST_ASSERT_EQ_STR("fmt 7 ok", snap.entries[2].message);
    nvs_error_log_copy(NULL);   // NULL 容忍

    return TEST_RESULT();
}
