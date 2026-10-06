// ================================================================
//  test_ui_disp_item.c — 数据项系统单测
//
//  覆盖:disp_item_read_value 的逐通道有效性哨兵、特殊格式化
//  (BAT/OILP/BKT/BOOST/AFR 的除数与小数)、扫描动画端点、自然量程
//  元数据、越界访问回退。label 类用例需要一个 dummy display ——
//  这里注册一个无渲染输出的最小 display 驱动。
// ================================================================

#include "test_util.h"
#include "ui_disp_item.h"
#include "ui_disp_item_logic.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "lvgl.h"

/** 注册最小 display(无 flush 实现),让 lv_label_create 可用。 */
static lv_display_t *setup_dummy_display(void)
{
    static uint8_t fb[64 * 8 * 2];   /* RGB565 */
    lv_display_t *disp = lv_display_create(360, 360);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

/** 帮手:全部通道给定有效典型值。 */
#define VALID_ARGS() 92, 38, 104, 42, 63, 13900, 35, 3210, 3456, 87, 123, 1470

/** 帮手:构造"其余通道有效、目标通道为指定哨兵"的参数组。 */
static bool read_one(disp_item_t item, int32_t target, int32_t *out)
{
    int16_t clt = 92, iat = 38, oil = 104, load = 42, tps = 63;
    int32_t bat = 13900;
    int16_t oilp = 35, brake = 3210, boost = 123, afr = 1470;
    uint16_t rpm = 3456;
    uint8_t speed = 87;
    switch (item) {
        case DISP_ITEM_CLT:   clt   = (int16_t)target; break;
        case DISP_ITEM_IAT:   iat   = (int16_t)target; break;
        case DISP_ITEM_OIL:   oil   = (int16_t)target; break;
        case DISP_ITEM_LOAD:  load  = (int16_t)target; break;
        case DISP_ITEM_TPS:   tps   = (int16_t)target; break;
        case DISP_ITEM_BAT:   bat   = target;          break;
        case DISP_ITEM_OILP:  oilp  = (int16_t)target; break;
        case DISP_ITEM_BKT:   brake = (int16_t)target; break;
        case DISP_ITEM_BOOST: boost = (int16_t)target; break;
        case DISP_ITEM_AFR:   afr   = (int16_t)target; break;
        default: return false;
    }
    return disp_item_read_value(item, clt, iat, oil, load, tps, bat, oilp,
                                brake, rpm, speed, boost, afr, out);
}

int main(void)
{
    lv_init();
    TEST_ASSERT(setup_dummy_display() != NULL);

    // ---- 元数据表 ----
    TEST_ASSERT_EQ_INT(18, DISP_ITEM_COUNT);   // M3: 12 原有 + TPMS×4 + G 力双轴
    TEST_ASSERT_EQ_STR("CLT", s_disp_meta[DISP_ITEM_CLT].name);
    TEST_ASSERT_EQ_STR("bar", s_disp_meta[DISP_ITEM_OILP].unit);
    TEST_ASSERT_EQ_STR("", s_disp_meta[DISP_ITEM_AFR].unit);
    TEST_ASSERT_EQ_STR("TPFL", s_disp_meta[DISP_ITEM_TPMS_FL].name);
    TEST_ASSERT_EQ_INT(10, s_needle_scale_meta[DISP_ITEM_TPMS_FL].div);
    TEST_ASSERT_EQ_INT(100, s_needle_scale_meta[DISP_ITEM_GFORCE_LAT].div);

    // 越界访问:name 返回 "",unit/color/range 回退 CLT
    TEST_ASSERT_EQ_STR("", ui_disp_item_name(200));
    TEST_ASSERT_EQ_STR("'C", ui_disp_item_unit(200));
    int32_t nmin, nmax, div;
    ui_disp_item_range(200, &nmin, &nmax, &div);
    TEST_ASSERT_EQ_INT(s_needle_scale_meta[DISP_ITEM_CLT].nmin, nmin);
    TEST_ASSERT_EQ_INT(s_needle_scale_meta[DISP_ITEM_CLT].nmax, nmax);

    // 自然量程的除数约定:raw = natural × div
    TEST_ASSERT_EQ_INT(1000, s_needle_scale_meta[DISP_ITEM_BAT].div);
    TEST_ASSERT_EQ_INT(10, s_needle_scale_meta[DISP_ITEM_OILP].div);
    TEST_ASSERT_EQ_INT(100, s_needle_scale_meta[DISP_ITEM_AFR].div);
    TEST_ASSERT_EQ_INT(1, s_needle_scale_meta[DISP_ITEM_RPM].div);

    // ---- read_value:有效值 ----
    int32_t out = 0;
    TEST_ASSERT(disp_item_read_value(DISP_ITEM_CLT, VALID_ARGS(), &out));
    TEST_ASSERT_EQ_INT(92, out);
    TEST_ASSERT(disp_item_read_value(DISP_ITEM_BOOST, VALID_ARGS(), &out));
    TEST_ASSERT_EQ_INT(123, out);

    // ---- read_value:各通道无效哨兵(ui.c 依赖这些边界) ----
    TEST_ASSERT(!read_one(DISP_ITEM_CLT, -40, &out));     // -40 = 无效
    TEST_ASSERT(read_one(DISP_ITEM_CLT, -39, &out));      // -39 仍是合法低温
    TEST_ASSERT_EQ_INT(-39, out);
    TEST_ASSERT(!read_one(DISP_ITEM_OIL, -100, &out));    // -100 = 无效
    TEST_ASSERT(!read_one(DISP_ITEM_LOAD, -1, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_TPS, -1, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_BAT, 0, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_OILP, -1, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_BKT, -1000, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_BOOST, -32768, &out));
    TEST_ASSERT(read_one(DISP_ITEM_BOOST, -32767, &out)); // 负压合法
    TEST_ASSERT(!read_one(DISP_ITEM_AFR, 799, &out));     // 区间 [800,2200]
    TEST_ASSERT(read_one(DISP_ITEM_AFR, 800, &out));
    TEST_ASSERT(!read_one(DISP_ITEM_AFR, 2201, &out));
    TEST_ASSERT(read_one(DISP_ITEM_AFR, 2200, &out));

    // ---- 扫描动画端点(r=0 起点 / r=1 峰值) ----
    TEST_ASSERT_EQ_INT(0, disp_item_sweep_value(DISP_ITEM_CLT, 0.0f));
    TEST_ASSERT_EQ_INT(120, disp_item_sweep_value(DISP_ITEM_CLT, 1.0f));
    TEST_ASSERT_EQ_INT(8000, disp_item_sweep_value(DISP_ITEM_RPM, 1.0f));
    TEST_ASSERT_EQ_INT(12000, disp_item_sweep_value(DISP_ITEM_BAT, 0.0f));
    TEST_ASSERT_EQ_INT(14400, disp_item_sweep_value(DISP_ITEM_BAT, 1.0f));
    TEST_ASSERT_EQ_INT(800, disp_item_sweep_value(DISP_ITEM_AFR, 0.0f));
    TEST_ASSERT_EQ_INT(2200, disp_item_sweep_value(DISP_ITEM_AFR, 1.0f));

    // ---- 格式化(真 label 上验证文本) ----
    lv_obj_t *label = lv_label_create(lv_scr_act());

    disp_item_set_text(label, DISP_ITEM_BAT, 13900, true);
    TEST_ASSERT_EQ_STR("13.9", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_OILP, 35, true);
    TEST_ASSERT_EQ_STR("3.5", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_BKT, 3210, true);
    TEST_ASSERT_EQ_STR("321", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_BOOST, -6, true);
    TEST_ASSERT_EQ_STR("-0.6", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_BOOST, 12, true);
    TEST_ASSERT_EQ_STR("1.2", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_AFR, 1470, true);
    TEST_ASSERT_EQ_STR("14.7", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_RPM, 3456, true);
    TEST_ASSERT_EQ_STR("3456", lv_label_get_text(label));
    disp_item_set_text(label, DISP_ITEM_CLT, 92, false);
    TEST_ASSERT_EQ_STR("--", lv_label_get_text(label));

    // ---- 报警着色:阈值来自 nvs_chart_alarm_get(mock 默认 OIP=8.0bar) ----
    // v9 的 lv_color_t 即 RGB888,直接 lv_color_eq 比较
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    disp_item_set_value_color(label, DISP_ITEM_OILP, 85, true);   // 8.5 ≥ 8.0 → 红
    TEST_ASSERT((lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), lv_color_hex(0xFF4D4D))));
    disp_item_set_value_color(label, DISP_ITEM_OILP, 79, true);   // 7.9 < 8.0 → 白
    TEST_ASSERT((lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), lv_color_hex(0xFFFFFF))));
    // 无效值不上色
    disp_item_set_value_color(label, DISP_ITEM_OILP, 999, false);
    TEST_ASSERT((lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), lv_color_hex(0xFFFFFF))));

    // ---- 自适应平滑:小差值 ±1 步进,大差值按 1/3 比例逼近 ----
    // (直接测 *_logic.h 的 static inline,不经过 label)
    TEST_ASSERT_EQ_INT(101, ui_disp_item_anim_step_i32(100, 102, 10));   // |diff|=2 ≤ 10 → +1
    TEST_ASSERT_EQ_INT(166, ui_disp_item_anim_step_i32(100, 300, 10));   // |diff|=200 → 200/3 = 66
    TEST_ASSERT_EQ_INT(68,  ui_disp_item_anim_step_i32(100, 2, 10));     // |diff|=98 → 98/3 = 32,负方向
    TEST_ASSERT_EQ_INT(100, ui_disp_item_anim_step_i32(100, 100, 10));   // 已到位不动
    TEST_ASSERT_EQ_INT(101, ui_disp_item_anim_step_i32(100, 101, 0));    // 大步路径 clamp 到 abs_diff,不越过目标
    TEST_ASSERT_EQ_INT(102, ui_disp_item_anim_step_i32(100, 102, 0));    // rapid=2 恰好到位
    int32_t state = 100;
    disp_item_update(&state, label, DISP_ITEM_CLT, 102, true, 10);
    TEST_ASSERT_EQ_INT(101, state);   // |diff|=2 ≤ 10 → +1
    state = 0;
    disp_item_update(&state, label, DISP_ITEM_CLT, 300, true, 10);
    TEST_ASSERT_EQ_INT(100, state);   // |diff|=300 → 300/3 = 100
    // 无效输入保持 state 不变(避免数据恢复时从 0 爬升)
    state = 250;
    disp_item_update(&state, label, DISP_ITEM_CLT, 999, false, 10);
    TEST_ASSERT_EQ_INT(250, state);
    // RPM 不做平滑(大范围快变量,平滑会显得"卡住")
    state = 1000;
    disp_item_update(&state, label, DISP_ITEM_RPM, 5600, true, 10);
    TEST_ASSERT_EQ_INT(5600, state);

    // ---- 报警判定(*_logic.h) ----
    TEST_ASSERT(ui_disp_item_alarm_over_threshold(80, 85, true));    // 越限
    TEST_ASSERT(ui_disp_item_alarm_over_threshold(80, 80, true));    // 等于阈值也算
    TEST_ASSERT(!ui_disp_item_alarm_over_threshold(80, 79, true));   // 未越限
    TEST_ASSERT(!ui_disp_item_alarm_over_threshold(80, 999, false)); // 无效值不报警
    TEST_ASSERT(!ui_disp_item_alarm_over_threshold(32767, 99999, true)); // 哨兵 = 报警关闭
    TEST_ASSERT(ui_disp_item_alarm_over_threshold(-5, -3, true));   // 负阈值按数值比较:-3 ≥ -5

    // ---- 统一缓存读取器(M3):先写缓存再读,含扩展通道 ----
    int32_t cv = 0;
    TEST_ASSERT(!ui_disp_item_read_cache(DISP_ITEM_CLT, &cv));       // 未写 → 哨兵无效
    obd_data_set_coolant_temp(92);
    TEST_ASSERT(ui_disp_item_read_cache(DISP_ITEM_CLT, &cv));
    TEST_ASSERT_EQ_INT(92, cv);
    obd_data_set_tpms_bar_x10(1, 228);
    TEST_ASSERT(ui_disp_item_read_cache(DISP_ITEM_TPMS_FR, &cv));
    TEST_ASSERT_EQ_INT(228, cv);
    TEST_ASSERT(!ui_disp_item_read_cache(DISP_ITEM_TPMS_RL, &cv));   // 未写轮无效
    obd_data_set_gforce_x100(87, -42);
    TEST_ASSERT(ui_disp_item_read_cache(DISP_ITEM_GFORCE_LAT, &cv));
    TEST_ASSERT_EQ_INT(87, cv);
    TEST_ASSERT(ui_disp_item_read_cache(DISP_ITEM_GFORCE_LON, &cv));
    TEST_ASSERT_EQ_INT(-42, cv);
    TEST_ASSERT(!ui_disp_item_read_cache((disp_item_t)99, &cv));     // 越界
    TEST_ASSERT(!ui_disp_item_read_cache(DISP_ITEM_RPM, NULL));

    return TEST_RESULT();
}
