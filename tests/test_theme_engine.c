// ================================================================
//  test_theme_engine.c — 主题引擎单测
//
//  覆盖三条加载路径(与固件行为一一对应):
//    1. 无 theme_0 分区 → theme_load_default() 回退(内置 default 调色板)
//    2. 合法 theme.bin(theme_store/boost_oil_example,v1 schema)→
//       manifest 元数据 / 颜色 / 页面清单正确加载
//    3. 损坏 theme.bin → 解析失败回退 default,不崩溃
//  再跑改造后的 theme_engine_test()(bool 返回)作为集成自检。
// ================================================================

#include "test_util.h"
#include "theme_engine/theme_interface.h"
#include "export_path/ui_theme.h"
#include "sim_platform.h"

#include <stdio.h>
#include <unistd.h>

/** 写一个损坏的"theme.bin"到临时目录并返回路径。 */
static const char *write_garbage_theme(void)
{
    static char path[256];
    snprintf(path, sizeof(path), "/tmp/obd_test_garbage_theme_%d.bin", (int)getpid());
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    fwrite("this is definitely not json \xff\xfe\x00garbage", 1, 40, f);
    fclose(f);
    return path;
}

int main(void)
{
    lv_init();
    theme_info_t info;

    // ---- 路径 1:无分区 → 默认回退 ----
    sim_theme_partition_load(NULL);
    TEST_ASSERT_EQ_INT(ESP_OK, theme_engine_init());
    TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
    TEST_ASSERT_EQ_STR("default", info.id);
    TEST_ASSERT_EQ_STR("DEFAULT", info.name);
    // 回退调色板来自编译期 default 主题(stub 与 theme.toml 一致)
    TEST_ASSERT_EQ_INT(0x000000, lv_color_to32(theme_get_color(UI_COLOR_BG)) & 0xFFFFFF);
    TEST_ASSERT_EQ_INT(0xFFFFFF, lv_color_to32(theme_get_color(UI_COLOR_RING)) & 0xFFFFFF);
    TEST_ASSERT_EQ_INT(0, theme_page_list_count());
    TEST_ASSERT(!theme_has_page("main_gauge"));
    TEST_ASSERT(!theme_has_page("logo"));       // 受保护页永不主题化
    TEST_ASSERT_EQ_INT(NULL == theme_get_asset("dial"), 1);
    TEST_ASSERT(theme_engine_test());

    // ---- 路径 2:合法 theme.bin(v1) ----
    sim_theme_partition_load("theme_store/boost_oil_example/theme.bin");
    TEST_ASSERT_EQ_INT(ESP_OK, theme_load(0));
    TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
    TEST_ASSERT_EQ_STR("boost_oil_example", info.id);
    TEST_ASSERT_EQ_STR("TURBO PRO", info.name);
    TEST_ASSERT_EQ_STR("1.0.0", info.version);
    // manifest colors: ring=0xFF6B00, arc_indicator=0xFFFF00, bg=0x0A0A0A
    // (比较 lv_color_t.full —— 与解析侧同一 RGB565 编码,避免量化位差)
    TEST_ASSERT(theme_get_color(UI_COLOR_RING).full == lv_color_hex(0xFF6B00).full);
    TEST_ASSERT(theme_get_color(UI_COLOR_ARC_INDICATOR).full == lv_color_hex(0xFFFF00).full);
    TEST_ASSERT(theme_get_color(UI_COLOR_BG).full == lv_color_hex(0x0A0A0A).full);
    // 页面清单:packer 注入了一个 main_gauge 自定义布局页
    TEST_ASSERT_EQ_INT(1, theme_page_list_count());
    TEST_ASSERT_EQ_STR("main_gauge", theme_page_list_at(0));
    TEST_ASSERT(theme_page_list_at(1) == NULL);
    TEST_ASSERT(theme_has_page("main_gauge"));
    // 受保护页仍然不可主题化
    TEST_ASSERT(!theme_has_page("logo"));
    TEST_ASSERT(!theme_has_page("intro"));
    TEST_ASSERT(!theme_has_page("boot_video"));
    TEST_ASSERT(theme_engine_test());

    // ---- 路径 3:损坏 theme.bin → 解析失败,回退 default ----
    const char *garbage = write_garbage_theme();
    TEST_ASSERT(garbage != NULL);
    sim_theme_partition_load(garbage);
    TEST_ASSERT_EQ_INT(ESP_OK, theme_engine_init());   // init 永不失败:回退 default
    TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
    TEST_ASSERT_EQ_STR("default", info.id);
    TEST_ASSERT_EQ_INT(0, theme_page_list_count());
    TEST_ASSERT(theme_engine_test());

    // ---- theme_unload 后可再次加载 ----
    theme_unload();
    TEST_ASSERT_EQ_INT(ESP_ERR_INVALID_STATE, theme_get_info(&info));
    sim_theme_partition_load("theme_store/boost_oil_example/theme.bin");
    TEST_ASSERT_EQ_INT(ESP_OK, theme_load(0));
    TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
    TEST_ASSERT_EQ_STR("boost_oil_example", info.id);

    return TEST_RESULT();
}
