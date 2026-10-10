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
#include "ui_fonts/ui_fonts.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "sim_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* ---- v2 组件编排(M3.3):构造一个最小 v2 theme.bin 并走完整加载链 ---- */
static const char *MANIFEST_V2 =
    "{"
    "\"schema_version\":\"2.0\","
    "\"theme\":{\"id\":\"v2test\",\"name\":\"V2\",\"version\":\"1.0.0\",\"author\":\"t\"},"
    "\"colors\":{"
    "\"bg\":\"0x000000\",\"ring\":\"0xFFFFFF\",\"arc_track\":\"0x333333\","
    "\"arc_indicator\":\"0xFFFFFF\",\"text_primary\":\"0xFFFFFF\","
    "\"text_secondary\":\"0x888888\",\"needle\":\"0xFF1010\",\"panel\":\"0x222222\"},"
    "\"components\":{"
    "\"mydial\":{\"size\":{\"w\":150,\"h\":150},\"elements\":["
    "{\"type\":\"label\",\"x\":10,\"y\":10,\"text\":\"HELLO\"}]}},"
    "\"pages\":{\"theme_pages\":[{"
    "\"id\":\"main_gauge\",\"type\":\"component_layout\","
    "\"layout_data_offset\":16384,\"layout_data_size\":%d}]}"
    "}";

static const char *LAYOUT_V2 =
    "{\"page_id\":\"main_gauge\",\"instances\":["
    "{\"component\":\"value\",\"channel\":\"obd.coolant_temp\",\"x\":10,\"y\":20,\"w\":150,\"h\":90},"
    "{\"component\":\"theme:mydial\",\"x\":10,\"y\":120,\"w\":150,\"h\":150},"
    "{\"component\":\"nope\",\"channel\":\"obd.rpm\",\"x\":0,\"y\":0,\"w\":50,\"h\":50}"
    "]}";

static const char *write_v2_theme(void)
{
    static char path[256];
    char manifest[2048];
    FILE *f;

    snprintf(path, sizeof(path), "/tmp/obd_test_v2_theme_%d.bin", (int)getpid());
    f = fopen(path, "wb");
    if (!f) return NULL;
    // 4MB 全 0xFF,manifest 置于偏移 0,layout 置于 16KB
    char *blob = malloc(4 * 1024 * 1024);
    if (!blob) { fclose(f); return NULL; }
    memset(blob, 0xFF, 4 * 1024 * 1024);
    size_t layout_len = strlen(LAYOUT_V2);
    snprintf(manifest, sizeof(manifest), MANIFEST_V2, (int)layout_len);
    memcpy(blob, manifest, strlen(manifest));
    memcpy(blob + 16384, LAYOUT_V2, layout_len);
    fwrite(blob, 1, 4 * 1024 * 1024, f);
    free(blob);
    fclose(f);
    return path;
}

/** 深度优先找指定文本的 label。 */
static bool find_label_text(lv_obj_t *obj, const char *text)
{
    if (!obj) return false;
    if (lv_obj_check_type(obj, &lv_label_class) &&
        strcmp(lv_label_get_text(obj), text) == 0) return true;
    for (int i = 0; i < lv_obj_get_child_cnt(obj); i++) {
        if (find_label_text(lv_obj_get_child(obj, i), text)) return true;
    }
    return false;
}

/** 注册最小 display(无渲染输出),让 theme_create_page 可创建对象。 */
static void setup_dummy_display(void)
{
    static uint8_t fb[64 * 8 * 2];   /* RGB565 */
    lv_display_t *disp = lv_display_create(360, 360);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_PARTIAL);
    (void)disp;
}

int main(void)
{
    lv_init();
    ui_fonts_init(360);   /* 组件取字体走 &ui_font_X,先填好 TinyTTF 字体 */
    setup_dummy_display();
    theme_info_t info;

    // ---- 路径 1:无分区 → 默认回退 ----
    sim_theme_partition_load(NULL);
    TEST_ASSERT_EQ_INT(ESP_OK, theme_engine_init());
    TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
    TEST_ASSERT_EQ_STR("default", info.id);
    TEST_ASSERT_EQ_STR("DEFAULT", info.name);
    // 回退调色板来自编译期 default 主题(stub 与 theme.toml 一致)
    TEST_ASSERT((lv_color_eq(theme_get_color(UI_COLOR_BG), lv_color_hex(0x000000))));
    TEST_ASSERT((lv_color_eq(theme_get_color(UI_COLOR_RING), lv_color_hex(0xFFFFFF))));
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
    // (v9 lv_color_t 即 RGB888,直接 lv_color_eq 比较)
    TEST_ASSERT((lv_color_eq(theme_get_color(UI_COLOR_RING), lv_color_hex(0xFF6B00))));
    TEST_ASSERT((lv_color_eq(theme_get_color(UI_COLOR_ARC_INDICATOR), lv_color_hex(0xFFFF00))));
    TEST_ASSERT((lv_color_eq(theme_get_color(UI_COLOR_BG), lv_color_hex(0x0A0A0A))));
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

    // ---- v2 组件编排(M3.3)----
    {
        const char *v2 = write_v2_theme();
        TEST_ASSERT(v2 != NULL);
        sim_theme_partition_load(v2);
        TEST_ASSERT_EQ_INT(ESP_OK, theme_load(0));
        TEST_ASSERT_EQ_INT(ESP_OK, theme_get_info(&info));
        TEST_ASSERT_EQ_STR("v2test", info.id);
        TEST_ASSERT_EQ_INT(1, theme_page_list_count());
        TEST_ASSERT_EQ_STR("main_gauge", theme_page_list_at(0));

        // 创建编排页:1 个内置 value 组件 + 1 个 theme:mydial(内含 label)
        lv_obj_t *page = theme_create_page("main_gauge");
        TEST_ASSERT(page != NULL);
        obd_data_set_coolant_temp(92);
        obd_snapshot_t snap;
        memset(&snap, 0, sizeof(snap));
        theme_update_data(&snap);
        TEST_ASSERT(find_label_text(page, "92"));     // value 组件经缓存刷新
        TEST_ASSERT(find_label_text(page, "HELLO")); // 主题组件原语
        TEST_ASSERT(!find_label_text(page, "nope")); // 未知组件被跳过
        lv_obj_del(page);
        // 清空 comp 实例避免悬垂(与固件页面删除路径一致)
    }

    return TEST_RESULT();
}
