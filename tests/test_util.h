#pragma once
// ================================================================
//  test_util.h — 主机端单测极简框架(M0)
//
//  零外部依赖:每个 test_*.c 自带 main(),以非零退出码表示失败,
//  由 CTest(tests/CMakeLists.txt)统一调度。
//  新代码的纯逻辑请抽到 *_logic.h(static inline)后在此测试 ——
//  这是 ref 仓库可测性的核心模式,见 docs/CODE_STYLE.md(M1)。
// ================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int t_checks = 0;
static int t_failed = 0;

/** 断言为真;失败时打印文件:行与表达式。 */
#define TEST_ASSERT(cond)                                                    \
    do {                                                                     \
        t_checks++;                                                          \
        if (!(cond)) {                                                       \
            t_failed++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                    \
    } while (0)

/** 整数相等断言,失败时打印两边实际值。 */
#define TEST_ASSERT_EQ_INT(expected, actual)                                 \
    do {                                                                     \
        long long e_ = (long long)(expected);                                \
        long long a_ = (long long)(actual);                                  \
        t_checks++;                                                          \
        if (e_ != a_) {                                                      \
            t_failed++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s == %s (expected %lld, got %lld)\n", \
                    __FILE__, __LINE__, #expected, #actual, e_, a_);         \
        }                                                                    \
    } while (0)

/** 字符串相等断言(NULL 视为 "<null>")。 */
#define TEST_ASSERT_EQ_STR(expected, actual)                                 \
    do {                                                                     \
        const char *e_ = (expected);                                         \
        const char *a_ = (actual);                                           \
        t_checks++;                                                          \
        if (!e_ || !a_ || strcmp(e_, a_) != 0) {                             \
            t_failed++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s == %s (expected \"%s\", got \"%s\")\n", \
                    __FILE__, __LINE__, #expected, #actual,                   \
                    e_ ? e_ : "<null>", a_ ? a_ : "<null>");                  \
        }                                                                    \
    } while (0)

/** 浮点近似断言(|a-b| <= eps)。 */
#define TEST_ASSERT_NEAR(expected, actual, eps)                              \
    do {                                                                     \
        double e_ = (double)(expected);                                      \
        double a_ = (double)(actual);                                        \
        t_checks++;                                                          \
        if (((a_ - e_) > (eps)) || ((e_ - a_) > (eps))) {                    \
            t_failed++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s ~= %s (expected %f, got %f, eps %f)\n", \
                    __FILE__, __LINE__, #expected, #actual, e_, a_, (double)(eps)); \
        }                                                                    \
    } while (0)

/** main() 末尾调用:打印统计并返回进程退出码。 */
#define TEST_RESULT()                                                        \
    (printf("%s: %d checks, %d failures\n", __FILE__, t_checks, t_failed),   \
     t_failed ? 1 : 0)
