// ================================================================
//  test_ui_component.c — 内置仪表组件单测(M3.2)
//
//  dummy display 上创建组件、写数据缓存、update 后断言渲染文本与
//  进度换算。视觉细节(间距/弧角度)由截图回归覆盖(模拟器)。
// ================================================================

#include "test_util.h"
#include "ui_component.h"
#include "app_obd_dsp/obd_data_cache.h"
#include "lvgl.h"

static lv_display_t *setup_dummy_display(void)
{
    static uint8_t fb[64 * 8 * 2];   /* RGB565 */
    lv_display_t *disp = lv_display_create(360, 360);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

static lv_obj_t *find_value_label(lv_obj_t *comp)
{
    const ui_comp_desc_t *d = ui_comp_desc_of(comp);
    int n = lv_obj_get_child_cnt(comp);
    for (int i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(comp, i);
        if (!lv_obj_check_type(c, &lv_label_class)) continue;
        if (d->type == UI_COMP_VALUE &&
            strcmp(lv_label_get_text(c), ui_disp_item_name((uint8_t)d->channel)) == 0) continue;
        if (d->type == UI_COMP_VALUE &&
            strcmp(lv_label_get_text(c), ui_disp_item_unit((uint8_t)d->channel)) == 0) continue;
        return c;
    }
    return NULL;
}

int main(void)
{
    lv_init();
    TEST_ASSERT(setup_dummy_display() != NULL);
    lv_obj_t *scr = lv_scr_act();
    TEST_ASSERT(scr != NULL);

    // ---- 类型名映射 ----
    TEST_ASSERT_EQ_INT(UI_COMP_VALUE, ui_comp_type_from_name("value"));
    TEST_ASSERT_EQ_INT(UI_COMP_ARC, ui_comp_type_from_name("arc"));
    TEST_ASSERT_EQ_INT(UI_COMP_BAR, ui_comp_type_from_name("bar"));
    TEST_ASSERT_EQ_INT(UI_COMP_BIG_NUM, ui_comp_type_from_name("bignum"));
    TEST_ASSERT_EQ_INT(UI_COMP_GFORCE, ui_comp_type_from_name("gforce"));
    TEST_ASSERT_EQ_INT(-1, ui_comp_type_from_name("nope"));
    TEST_ASSERT_EQ_INT(-1, ui_comp_type_from_name(NULL));
    TEST_ASSERT_EQ_STR("arc", ui_comp_type_name(UI_COMP_ARC));

    // ---- 防御式 ----
    ui_comp_desc_t d = { .type = UI_COMP_VALUE, .channel = DISP_ITEM_CLT, .x = 0, .y = 0, .w = 100, .h = 80 };
    TEST_ASSERT(ui_comp_create(NULL, scr) == NULL);
    TEST_ASSERT(ui_comp_create(&d, NULL) == NULL);
    d.type = (ui_comp_type_t)99;          TEST_ASSERT(ui_comp_create(&d, scr) == NULL);
    d.type = UI_COMP_VALUE; d.channel = (disp_item_t)99; TEST_ASSERT(ui_comp_create(&d, scr) == NULL);
    d.channel = DISP_ITEM_CLT; d.w = 0;   TEST_ASSERT(ui_comp_create(&d, scr) == NULL);
    TEST_ASSERT(!ui_comp_update(NULL));
    TEST_ASSERT(ui_comp_desc_of(NULL) == NULL);

    // ---- VALUE:无效 → "--",有效 → 除数格式化 ----
    d.w = 100; d.h = 80;
    lv_obj_t *v = ui_comp_create(&d, scr);
    TEST_ASSERT(v != NULL);
    TEST_ASSERT(!ui_comp_update(v));      // CLT 未写 → 无效
    TEST_ASSERT_EQ_STR("--", lv_label_get_text(find_value_label(v)));   // 首个无效也要刷占位
    obd_data_set_coolant_temp(92);
    TEST_ASSERT(ui_comp_update(v));
    TEST_ASSERT_EQ_STR("92", lv_label_get_text(find_value_label(v)));
    obd_data_set_tpms_bar_x10(2, 228);
    d.channel = DISP_ITEM_TPMS_RL;
    lv_obj_t *tp = ui_comp_create(&d, scr);
    TEST_ASSERT(tp != NULL);
    TEST_ASSERT(ui_comp_update(tp));
    TEST_ASSERT_EQ_STR("22.8", lv_label_get_text(find_value_label(tp)));   // 0.1bar ÷10

    // ---- BIG_NUM:负值 G 力格式 ----
    obd_data_set_gforce_x100(87, -42);
    d.type = UI_COMP_BIG_NUM; d.channel = DISP_ITEM_GFORCE_LAT; d.w = 160; d.h = 120;
    lv_obj_t *bn = ui_comp_create(&d, scr);
    TEST_ASSERT(bn != NULL);
    TEST_ASSERT(ui_comp_update(bn));
    TEST_ASSERT_EQ_STR("0.87", lv_label_get_text(find_value_label(bn)));

    // ---- BAR:进度换算(CLT -20..130,92°C → 64%) ----
    d.type = UI_COMP_BAR; d.channel = DISP_ITEM_CLT; d.w = 200; d.h = 60;
    lv_obj_t *bar = ui_comp_create(&d, scr);
    TEST_ASSERT(bar != NULL);
    TEST_ASSERT(ui_comp_update(bar));
    // 找到 lv_bar 子对象断言进度
    int pct = -1;
    for (int i = 0; i < lv_obj_get_child_cnt(bar); i++) {
        lv_obj_t *c = lv_obj_get_child(bar, i);
        if (lv_obj_check_type(c, &lv_bar_class)) pct = lv_bar_get_value(c);
    }
    TEST_ASSERT_EQ_INT(74, pct);          // (92-(-20))/150 = 74.7 → 74

    // ---- 首写零值必须渲染(SPD 静止为 0,不能停留占位文本) ----
    d.type = UI_COMP_VALUE; d.channel = DISP_ITEM_SPEED; d.x = 0; d.y = 200; d.w = 100; d.h = 80;
    lv_obj_t *spd = ui_comp_create(&d, scr);
    TEST_ASSERT(spd != NULL);
    TEST_ASSERT(ui_comp_update(spd));
    TEST_ASSERT_EQ_STR("0", lv_label_get_text(find_value_label(spd)));

    // ---- GFORCE:双通道驱动,点在圆内移动 ----
    d.type = UI_COMP_GFORCE; d.channel = DISP_ITEM_GFORCE_LAT; d.w = 120; d.h = 120;
    lv_obj_t *gf = ui_comp_create(&d, scr);
    TEST_ASSERT(gf != NULL);
    TEST_ASSERT(ui_comp_update(gf));      // 已写 87/-42

    // ---- 描述符可取回 ----
    const ui_comp_desc_t *back = ui_comp_desc_of(v);
    TEST_ASSERT(back != NULL);
    TEST_ASSERT_EQ_INT(UI_COMP_VALUE, back->type);
    TEST_ASSERT_EQ_INT(DISP_ITEM_CLT, back->channel);

    return TEST_RESULT();
}
