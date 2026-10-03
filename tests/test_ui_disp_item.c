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
#include "lvgl.h"

/** 注册最小 display(无 flush 实现),让 lv_label_create 可用。 */
static lv_disp_t *setup_dummy_display(void)
{
    static lv_disp_draw_buf_t buf;
    static lv_color_t fb[64 * 8];
    static lv_disp_drv_t drv;
    lv_disp_draw_buf_init(&buf, fb, NULL, 64 * 8);
    lv_disp_drv_init(&drv);
    drv.hor_res = 360;
    drv.ver_res = 360;
    drv.draw_buf = &buf;
    return lv_disp_drv_register(&drv);
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
    TEST_ASSERT_EQ_INT(12, DISP_ITEM_COUNT);
    TEST_ASSERT_EQ_STR("CLT", s_disp_meta[DISP_ITEM_CLT].name);
    TEST_ASSERT_EQ_STR("bar", s_disp_meta[DISP_ITEM_OILP].unit);
    TEST_ASSERT_EQ_STR("", s_disp_meta[DISP_ITEM_AFR].unit);

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
    // 比较用 lv_color_t.full(与被测代码同一 RGB565 编码,避免量化位差)
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    disp_item_set_value_color(label, DISP_ITEM_OILP, 85, true);   // 8.5 ≥ 8.0 → 红
    TEST_ASSERT(lv_obj_get_style_text_color(label, LV_PART_MAIN).full
                == lv_color_hex(0xFF4D4D).full);
    disp_item_set_value_color(label, DISP_ITEM_OILP, 79, true);   // 7.9 < 8.0 → 白
    TEST_ASSERT(lv_obj_get_style_text_color(label, LV_PART_MAIN).full
                == lv_color_hex(0xFFFFFF).full);
    // 无效值不上色
    disp_item_set_value_color(label, DISP_ITEM_OILP, 999, false);
    TEST_ASSERT(lv_obj_get_style_text_color(label, LV_PART_MAIN).full
                == lv_color_hex(0xFFFFFF).full);

    // ---- 自适应平滑:小差值 ±1 步进,大差值按 1/3 比例逼近 ----
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

    return TEST_RESULT();
}
