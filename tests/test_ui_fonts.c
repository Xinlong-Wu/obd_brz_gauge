/* test_ui_fonts.c — 字号换算纯逻辑:8 档 × 3 分辨率与旧 lv_font_conv
 * 位图产物(gen_fonts.py target_px)逐值一致,保证 TinyTTF 切换后各档
 * 视觉大小不变。期望值来源:旧生成文件头 "target px" 注释逐档抄录。 */
#include "test_util.h"

#include "ui_fonts/ui_fonts_logic.h"

/* {240, 360, 466} 三档期望 px,行序对应 nominal {16,20,24,36,40,44,56,140} */
static const int expected[UI_FONT_COUNT][3] = {
    {11, 16, 21},   /* 16  */
    {13, 20, 26},   /* 20  */
    {16, 24, 31},   /* 24  */
    {24, 36, 47},   /* 36  */
    {27, 40, 52},   /* 40  */
    {29, 44, 57},   /* 44  */
    {37, 56, 72},   /* 56  */
    {93, 140, 181}, /* 140 */
};
static const int res_list[3] = {240, 360, 466};

int main(void)
{
    for (int i = 0; i < UI_FONT_COUNT; i++) {
        for (int r = 0; r < 3; r++) {
            int px = ui_font_px(ui_font_nominal_sizes[i], res_list[r]);
            TEST_ASSERT_EQ_INT(expected[i][r], px);
        }
    }

    // 下限保护:极小分辨率不产出 <8px
    TEST_ASSERT_EQ_INT(8, ui_font_px(16, 60));

    return TEST_RESULT();
}
