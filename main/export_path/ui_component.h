#pragma once
// ================================================================
//  ui_component.h — 内置仪表组件注册表(M3.2)
//
//  组件 = 类型 + 通道绑定 + 槽位矩形:渲染细节封在各类型的
//  create/update 里,皮肤一律取 ui_theme_color_lv 角色(主题换肤即生效),
//  数据一律经 ui_disp_item_read_cache(统一通道,M3.1)。
//  消费方:主题编排页(theme.bin v2,M3.3)与用户自定义仪表页(M4)。
// ================================================================

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui_disp_item.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_COMP_VALUE = 0,   // 值卡:名称 + 大数字 + 单位(元数据自动填充)
    UI_COMP_ARC,         // 弧表:圆弧进度 + 中央数值
    UI_COMP_BAR,         // 横条:进度条 + 右侧数值
    UI_COMP_BIG_NUM,     // 纯大数字(挡位/大字号场景)
    UI_COMP_GFORCE,      // G 力点图(双通道 lat/lon,画点 + 短轨迹)
    UI_COMP_TYPE_COUNT
} ui_comp_type_t;

/** 组件实例描述(编排层构造,create 消费)。 */
typedef struct {
    ui_comp_type_t type;
    disp_item_t    channel;     // GFORCE 组件忽略单通道,固定用 GFORCE_LAT/LON
    int16_t        x, y, w, h;  // 槽位矩形(父页面坐标)
} ui_comp_desc_t;

/** 类型名 ↔ 枚举("value"/"arc"/"bar"/"bignum"/"gforce");未知返回 -1。 */
int ui_comp_type_from_name(const char *name);
const char *ui_comp_type_name(ui_comp_type_t type);

/** 创建组件(挂到 parent)。失败返回 NULL。 */
lv_obj_t *ui_comp_create(const ui_comp_desc_t *desc, lv_obj_t *parent);

/** 数据刷新:从数据缓存读通道值并更新渲染。返回是否有新值。 */
bool ui_comp_update(lv_obj_t *comp);

/** 组件当前绑定的类型/通道(desc 存在 obj->user_data)。NULL 安全。 */
const ui_comp_desc_t *ui_comp_desc_of(lv_obj_t *comp);

#ifdef __cplusplus
}
#endif
