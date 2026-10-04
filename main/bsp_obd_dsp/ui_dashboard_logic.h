#pragma once
// ================================================================
//  ui_dashboard_logic.h — 用户自定义仪表页纯逻辑(M4,static inline)
//
//  职责:配置清洗(损坏 NVS 自愈)、老设备迁移(按现有 TEMP/INFO/
//  NEEDLE/CHART 映射生成初始页,外观对齐现轮播)、平铺数计算。
//  渲染在 ui_home_runtime.c;此处零依赖可单测。
// ================================================================

#include <string.h>

#include "bsp_obd_dsp/nvs_storage.h"
// 槽项有效性判据用统一通道词汇表(disp_item_t)。bsp→export_path 的这层
// 依赖是有意取舍:词汇表是全仓单一事实源,避免在两处硬编码 18。
#include "export_path/ui_disp_item.h"

/** 首页平铺:MENU(0) + 仪表页(1..N) + ADD(N+1)。 */
#define UI_HOME_PAGE_MENU 0u

static inline uint8_t ui_dashboard_logic_tile_count(uint8_t page_count)
{
    if (page_count > UI_DASHBOARD_MAX_PAGES) page_count = UI_DASHBOARD_MAX_PAGES;
    return (uint8_t)(1u + page_count + 1u);
}

/** 平铺索引 → 仪表页下标(0 基);MENU/ADD 平铺返回 page_count(越界哨兵)。 */
static inline uint8_t ui_dashboard_logic_tile_to_page(uint8_t tile, uint8_t page_count)
{
    if (tile == 0u || tile >= (uint8_t)(page_count + 1u)) return page_count;
    return (uint8_t)(tile - 1u);
}

/** 单页清洗:槽位数/槽项/页类型钳制;返回 false 表示该页结构已无效。 */
static inline bool ui_dashboard_logic_sanitize_page(ui_dashboard_page_cfg_t *page)
{
    if (page == NULL) return false;
    if (page->type >= (uint8_t)UI_DASHBOARD_PAGE_TYPE_COUNT) page->type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    if (page->slot_count == 0u || page->slot_count > UI_DASHBOARD_MAX_SLOTS) {
        page->slot_count = 1u;
    }
    for (uint8_t i = 0u; i < page->slot_count; i++) {
        if (page->slot_items[i] >= (uint8_t)DISP_ITEM_COUNT) {
            page->slot_items[i] = (uint8_t)DISP_ITEM_CLT;
        }
    }
    for (uint8_t i = page->slot_count; i < UI_DASHBOARD_MAX_SLOTS; i++) {
        page->slot_items[i] = 0u;
    }
    return true;
}

/** 整表清洗:版本不符整体重置为待迁移态;返回 true = 结构可用。 */
static inline bool ui_dashboard_logic_sanitize(ui_dashboard_cfg_t *cfg)
{
    if (cfg == NULL) return false;

    if (cfg->version != UI_DASHBOARD_VERSION) {
        memset(cfg, 0, sizeof(*cfg));
        cfg->version = UI_DASHBOARD_VERSION;
        return true;   // page_count == 0 → 待迁移
    }

    if (cfg->page_count > UI_DASHBOARD_MAX_PAGES) cfg->page_count = UI_DASHBOARD_MAX_PAGES;
    for (uint8_t i = 0u; i < cfg->page_count; i++) {
        (void)ui_dashboard_logic_sanitize_page(&cfg->pages[i]);
    }
    if (cfg->default_page > cfg->page_count) cfg->default_page = 0u;   // 0=MENU
    return true;
}

/**
 * 老设备迁移:page_count == 0 时,按现有 NVS 显示映射生成初始仪表页,
 * 与替换前的静态轮播一一对应(TEMP/INFO/CHART/NEEDLE/GEAR/RPM/SPEED)。
 * 已迁移配置(page_count > 0)不动。返回是否发生了迁移。
 */
static inline bool ui_dashboard_logic_migrate_from_maps(ui_dashboard_cfg_t *cfg,
                                                         const uint8_t temp_map[3],
                                                         const uint8_t info_map[5],
                                                         uint8_t needle_idx,
                                                         uint8_t chart_idx)
{
    if (cfg == NULL || cfg->page_count != 0u) return false;
    if (needle_idx >= (uint8_t)DISP_ITEM_COUNT) needle_idx = (uint8_t)DISP_ITEM_CLT;
    if (chart_idx >= (uint8_t)DISP_ITEM_COUNT) chart_idx = (uint8_t)DISP_ITEM_OILP;

    ui_dashboard_page_cfg_t *p;
    uint8_t next = 0u;

    // TEMP:3 行值卡(temp_display_map 原序)
    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    p->slot_count = 3u;
    for (uint8_t i = 0u; i < 3u; i++) {
        p->slot_items[i] = (temp_map && temp_map[i] < (uint8_t)DISP_ITEM_COUNT)
                               ? temp_map[i] : (uint8_t)i;
    }

    // INFO:5 格值卡(info_display_map 原序)
    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    p->slot_count = 5u;
    for (uint8_t i = 0u; i < 5u; i++) {
        p->slot_items[i] = (info_map && info_map[i] < (uint8_t)DISP_ITEM_COUNT)
                               ? info_map[i] : (uint8_t)i;
    }

    // CHART:曲线源作为单槽 METRIC(曲线组件 M4.b 渲染)
    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    p->slot_count = 1u;
    p->slot_items[0] = chart_idx;

    // NEEDLE:单槽大值卡(指针视图 M4.b 用 arc 组件呈现)
    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_METRIC;
    p->slot_count = 1u;
    p->slot_items[0] = needle_idx;

    // GEAR / GFORCE:专用页类型
    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_GEAR;

    p = &cfg->pages[next++];
    memset(p, 0, sizeof(*p));
    p->type = (uint8_t)UI_DASHBOARD_PAGE_GFORCE;

    cfg->page_count = next;   // 6 页(RPM/SPEED 并入用户自建,保持轻量)
    cfg->default_page = 1u;   // 开机停 TEMP 页
    return true;
}
