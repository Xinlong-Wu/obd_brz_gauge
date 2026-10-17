// ================================================================
//  test_jdm_fonts.c — JDM 字体表纯逻辑单测
//
//  覆盖:角色表(家族/母版字号)与设计系统阶梯一致、
//  jdm_font_px 三档分辨率换算、init 后角色指针有效性。
// ================================================================

#include "test_util.h"
#include "lvgl.h"
#include "core/jdm_fonts.h"

int main(void)
{
    // ---- 字号阶梯与 ui-prototype/foundations.js 一致 ----
    TEST_ASSERT_EQ_INT(180, JDM_FONT_SPECS[JDM_FONT_HERO].master_px);
    TEST_ASSERT_EQ_INT(144, JDM_FONT_SPECS[JDM_FONT_ECCENTRIC].master_px);
    TEST_ASSERT_EQ_INT(96,  JDM_FONT_SPECS[JDM_FONT_ANALOG].master_px);
    TEST_ASSERT_EQ_INT(90,  JDM_FONT_SPECS[JDM_FONT_BOOST].master_px);
    TEST_ASSERT_EQ_INT(82,  JDM_FONT_SPECS[JDM_FONT_PAIR].master_px);
    TEST_ASSERT_EQ_INT(80,  JDM_FONT_SPECS[JDM_FONT_TYRE].master_px);
    TEST_ASSERT_EQ_INT(76,  JDM_FONT_SPECS[JDM_FONT_COMPACT].master_px);
    TEST_ASSERT_EQ_INT(60,  JDM_FONT_SPECS[JDM_FONT_MEDIA_TITLE].master_px);
    TEST_ASSERT_EQ_INT(50,  JDM_FONT_SPECS[JDM_FONT_STATISTIC].master_px);
    TEST_ASSERT_EQ_INT(30,  JDM_FONT_SPECS[JDM_FONT_BODY].master_px);
    TEST_ASSERT_EQ_INT(28,  JDM_FONT_SPECS[JDM_FONT_TITLE].master_px);
    TEST_ASSERT_EQ_INT(26,  JDM_FONT_SPECS[JDM_FONT_LABEL].master_px);
    TEST_ASSERT_EQ_INT(24,  JDM_FONT_SPECS[JDM_FONT_SUPPORT].master_px);
    TEST_ASSERT_EQ_INT(22,  JDM_FONT_SPECS[JDM_FONT_SMALL].master_px);

    // ---- 家族绑定:大数字 Barlow,标签/单位 Mono ----
    TEST_ASSERT_EQ_INT(JDM_FAM_BARLOW,      JDM_FONT_SPECS[JDM_FONT_HERO].family);
    TEST_ASSERT_EQ_INT(JDM_FAM_BARLOW_SEMI, JDM_FONT_SPECS[JDM_FONT_PAIR].family);
    TEST_ASSERT_EQ_INT(JDM_FAM_BARLOW_SEMI, JDM_FONT_SPECS[JDM_FONT_MEDIA_TITLE].family);
    TEST_ASSERT_EQ_INT(JDM_FAM_MONO,        JDM_FONT_SPECS[JDM_FONT_SMALL].family);
    TEST_ASSERT_EQ_INT(JDM_FAM_MONO_MED,    JDM_FONT_SPECS[JDM_FONT_LABEL].family);

    // ---- px 换算:720 母版 → 240/360/466(四舍五入) ----
    TEST_ASSERT_EQ_INT(90,  jdm_font_px(180, 360));
    TEST_ASSERT_EQ_INT(60,  jdm_font_px(180, 240));
    TEST_ASSERT_EQ_INT(117, jdm_font_px(180, 466));   // 180*466/720 = 116.5 → 117
    TEST_ASSERT_EQ_INT(13,  jdm_font_px(26, 360));
    TEST_ASSERT_EQ_INT(9,   jdm_font_px(22, 300));    // 22*300/720 = 9.17 → 9
    TEST_ASSERT_EQ_INT(15,  jdm_font_px(30, 360));

    // ---- v18 semantic metadata is opt-in; runtime roles above stay legacy ----
    TEST_ASSERT_EQ_INT(180, jdm_type_spec(JDM_TYPE_ROLE_VALUE_PRIMARY).master_px);
    TEST_ASSERT_EQ_INT(80, jdm_type_spec(JDM_TYPE_ROLE_VALUE_COMPACT).master_px);
    TEST_ASSERT_EQ_INT(50, jdm_type_spec(JDM_TYPE_ROLE_VALUE_SECONDARY).master_px);
    TEST_ASSERT_EQ_INT(36, jdm_type_spec(JDM_TYPE_ROLE_PAGE_TITLE).master_px);
    TEST_ASSERT_EQ_INT(32, jdm_type_spec(JDM_TYPE_ROLE_LABEL).master_px);
    TEST_ASSERT_EQ_INT(28, jdm_type_spec(JDM_TYPE_ROLE_UNIT).master_px);
    TEST_ASSERT_EQ_INT(JDM_FAM_BARLOW_SEMI, jdm_type_spec(JDM_TYPE_ROLE_PAIR_CODE).family);
    TEST_ASSERT_EQ_INT(0, jdm_type_spec(JDM_TYPE_ROLE_COUNT).master_px);

    // ---- init 前:返回默认字体兜底 ----
    TEST_ASSERT(jdm_font(JDM_FONT_HERO) == LV_FONT_DEFAULT);
    TEST_ASSERT(jdm_font(JDM_FONT_COUNT) == LV_FONT_DEFAULT);   // 越界兜底

    // ---- init 后:角色指针可用(不校验字形,栅格化在绘制期) ----
    jdm_fonts_init(360);
    for (int i = 0; i < JDM_FONT_COUNT; i++) {
        TEST_ASSERT(jdm_font((jdm_font_role_t)i) != NULL);
        TEST_ASSERT(jdm_font((jdm_font_role_t)i)->line_height > 0);
    }

    if (t_failed) {
        fprintf(stderr, "%d/%d checks failed\n", t_failed, t_checks);
        return 1;
    }
    printf("test_jdm_fonts: %d checks passed\n", t_checks);
    return 0;
}
