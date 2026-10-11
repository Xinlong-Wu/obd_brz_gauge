// ================================================================
//  jdm_shell.c — 表盘底座实现
// ================================================================

#include "jdm_shell.h"

#include "core/jdm_geo.h"
#include "core/jdm_tokens.h"
#include "core/jdm_theme.h"

lv_obj_t *jdm_shell_build(lv_obj_t *parent)
{
    // face 底色圆(r=360)
    lv_obj_t *face = lv_obj_create(parent);
    lv_obj_remove_style_all(face);
    lv_obj_set_pos(face, 0, 0);
    lv_obj_set_size(face, UIS(720), UIS(720));
    lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(face, jdm_color(JDM_ROLE_FACE), 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);

    // 刻度环(r=342,line 色 2px)
    lv_obj_t *rim = lv_obj_create(face);
    lv_obj_remove_style_all(rim);
    lv_obj_set_size(rim, UIS(684), UIS(684));
    lv_obj_center(rim);
    lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(rim, jdm_color(JDM_ROLE_LINE), 0);
    lv_obj_set_style_border_width(rim, UIS(2), 0);
    lv_obj_set_style_bg_opa(rim, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(rim, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 顶部定位标记 rect(356,32,8×24) rx=3 amber
    lv_obj_t *mark = lv_obj_create(face);
    lv_obj_remove_style_all(mark);
    lv_obj_set_size(mark, UIS(8), UIS(24));
    lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, UIS(32));
    lv_obj_set_style_radius(mark, UIS(3), 0);
    lv_obj_set_style_bg_color(mark, jdm_color(JDM_ROLE_AMBER), 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 内容层(全屏,覆盖 face;页面控件全建这里)
    lv_obj_t *content = lv_obj_create(face);
    lv_obj_remove_style_all(content);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, UIS(720), UIS(720));
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    return content;
}
