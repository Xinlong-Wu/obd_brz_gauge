// ================================================================
//  test_ui_dashboard.c — 用户自定义仪表页数据模型单测(M4.a)
//
//  覆盖:平铺数/索引换算、损坏配置清洗、老设备映射迁移
//  (TEMP/INFO/CHART/NEEDLE 按现有 NVS 映射生成,GEAR/GFORCE 专用页)。
// ================================================================

#include "test_util.h"
#include "bsp_obd_dsp/nvs_storage.h"
#include "bsp_obd_dsp/ui_dashboard_logic.h"

int main(void)
{
    // ---- 平铺数:MENU + N 页 + ADD ----
    TEST_ASSERT_EQ_INT(3, ui_dashboard_logic_tile_count(1));    // MENU+1+ADD
    TEST_ASSERT_EQ_INT(10, ui_dashboard_logic_tile_count(8));   // 上限
    TEST_ASSERT_EQ_INT(10, ui_dashboard_logic_tile_count(99));  // 越界钳制

    // ---- 平铺索引 → 仪表页下标 ----
    TEST_ASSERT_EQ_INT(6, ui_dashboard_logic_tile_to_page(0, 6));   // MENU → 哨兵
    TEST_ASSERT_EQ_INT(0, ui_dashboard_logic_tile_to_page(1, 6));
    TEST_ASSERT_EQ_INT(5, ui_dashboard_logic_tile_to_page(6, 6));
    TEST_ASSERT_EQ_INT(6, ui_dashboard_logic_tile_to_page(7, 6));   // ADD 平铺 → 哨兵

    // ---- 清洗:版本不符整体重置为待迁移态 ----
    ui_dashboard_cfg_t cfg;
    memset(&cfg, 0xAA, sizeof(cfg));
    cfg.version = 99;
    TEST_ASSERT(ui_dashboard_logic_sanitize(&cfg));
    TEST_ASSERT_EQ_INT(UI_DASHBOARD_VERSION, cfg.version);
    TEST_ASSERT_EQ_INT(0, cfg.page_count);

    // ---- 清洗:结构合法但值越界 ----
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = UI_DASHBOARD_VERSION;
    cfg.page_count = 2;
    cfg.default_page = 5;                              // 越界 → 回 MENU
    cfg.pages[0].type = 99;                            // 越界类型 → METRIC
    cfg.pages[0].slot_count = 9;                       // 越界槽数 → 1
    cfg.pages[1].slot_count = 2;
    cfg.pages[1].slot_items[0] = 200;                  // 越界槽项 → CLT
    cfg.pages[1].slot_items[1] = (uint8_t)DISP_ITEM_RPM;
    TEST_ASSERT(ui_dashboard_logic_sanitize(&cfg));
    TEST_ASSERT_EQ_INT(0, cfg.default_page);
    TEST_ASSERT_EQ_INT(0, cfg.pages[0].type);
    TEST_ASSERT_EQ_INT(1, cfg.pages[0].slot_count);
    TEST_ASSERT_EQ_INT((int)DISP_ITEM_CLT, cfg.pages[1].slot_items[0]);
    TEST_ASSERT_EQ_INT((int)DISP_ITEM_RPM, cfg.pages[1].slot_items[1]);
    TEST_ASSERT(!ui_dashboard_logic_sanitize(NULL));

    // ---- 迁移:老设备(page_count==0)按现有映射生成 6 页 ----
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = UI_DASHBOARD_VERSION;
    {
        uint8_t temp_map[3] = {0, 1, 2};          // CLT/IAT/OIL
        uint8_t info_map[5] = {0, 2, 3, 4, 1};    // CLT/OIL/LOAD/TPS/IAT
        TEST_ASSERT(ui_dashboard_logic_migrate_from_maps(&cfg, temp_map, info_map,
                                                          (uint8_t)DISP_ITEM_BOOST,
                                                          (uint8_t)DISP_ITEM_OILP));
        TEST_ASSERT_EQ_INT(6, cfg.page_count);
        TEST_ASSERT_EQ_INT(1, cfg.default_page);          // 开机停 TEMP

        // 第 1 页 TEMP:3 槽 = temp_map 原序
        TEST_ASSERT_EQ_INT(3, cfg.pages[0].slot_count);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_CLT, cfg.pages[0].slot_items[0]);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_IAT, cfg.pages[0].slot_items[1]);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_OIL, cfg.pages[0].slot_items[2]);

        // 第 2 页 INFO:5 槽 = info_map 原序
        TEST_ASSERT_EQ_INT(5, cfg.pages[1].slot_count);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_OIL, cfg.pages[1].slot_items[1]);

        // 第 3 页 CHART 源 / 第 4 页 NEEDLE 源
        TEST_ASSERT_EQ_INT(1, cfg.pages[2].slot_count);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_OILP, cfg.pages[2].slot_items[0]);
        TEST_ASSERT_EQ_INT((int)DISP_ITEM_BOOST, cfg.pages[3].slot_items[0]);

        // 第 5/6 页 GEAR / GFORCE
        TEST_ASSERT_EQ_INT((int)UI_DASHBOARD_PAGE_GEAR, cfg.pages[4].type);
        TEST_ASSERT_EQ_INT((int)UI_DASHBOARD_PAGE_GFORCE, cfg.pages[5].type);

        // 已迁移配置不动
        TEST_ASSERT(!ui_dashboard_logic_migrate_from_maps(&cfg, temp_map, info_map, 0, 0));
        TEST_ASSERT_EQ_INT(6, cfg.page_count);
    }

    // ---- 迁移:映射 NULL/越界时的兜底 ----
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = UI_DASHBOARD_VERSION;
    TEST_ASSERT(ui_dashboard_logic_migrate_from_maps(&cfg, NULL, NULL, 200, 200));
    TEST_ASSERT_EQ_INT((int)DISP_ITEM_CLT, cfg.pages[0].slot_items[0]);   // temp 兜底 0,1,2
    TEST_ASSERT_EQ_INT((int)DISP_ITEM_OILP, cfg.pages[2].slot_items[0]);  // chart 越界 → OILP
    TEST_ASSERT_EQ_INT((int)DISP_ITEM_CLT, cfg.pages[3].slot_items[0]);   // needle 越界 → CLT

    return TEST_RESULT();
}
