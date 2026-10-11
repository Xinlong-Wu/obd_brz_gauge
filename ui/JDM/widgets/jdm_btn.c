// ================================================================
//  jdm_btn.c — 动作按钮实现
//
//  八角切角 = 横条 (0,cut,w,h−2cut) ∪ 竖条 (cut,0,w−2cut,h) 两块
//  填充(几何上恰为切掉四角的八边形),描边为一条 9 点闭合折线。
// ================================================================

#include "jdm_btn.h"

#include <string.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_fonts.h"
#include "core/jdm_tokens.h"
#include "core/jdm_draw.h"

// 按下填充色(components.css --control-pressed #26343a)
#define JDM_HEX_PRESSED 0x26343A

#define MAX_LINES 2

typedef struct {
    lv_obj_t *fill[2];      // 横条/竖条(ghost 为 NULL)
    lv_obj_t *label[MAX_LINES];
    int nlabel;
    bool ghost;
} btn_t;

static void fill_color(btn_t *b, lv_color_t c)
{
    if (b->ghost) return;
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_bg_color(b->fill[i], c, 0);
    }
}

static void press_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    btn_t *b = lv_event_get_user_data(e);
    if (code == LV_EVENT_PRESSED) {
        fill_color(b, lv_color_hex(JDM_HEX_PRESSED));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        fill_color(b, jdm_color(JDM_ROLE_PANEL));
    }
}

static void delete_cb(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

// 在副本上以 '\0' 截断拆行(原串不可写);out[i] 指向各行首
static int split_lines(char *label, const char *out[MAX_LINES])
{
    int n = 1;
    out[0] = label ? label : "";
    for (char *p = label ? label : (char *)""; *p; p++) {
        if (*p == '\n' && n < MAX_LINES) {
            *p = '\0';
            out[n++] = p + 1;
        }
    }
    return n;
}

#define LABEL_BUF 64

static int dup_lines(const char *label, char *tmp, const char *out[MAX_LINES])
{
    size_t len = strlen(label ? label : "");
    if (len >= LABEL_BUF) len = LABEL_BUF - 1;
    memcpy(tmp, label ? label : "", len);
    tmp[len] = '\0';
    return split_lines(tmp, out);
}

lv_obj_t *jdm_btn_create_ex(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                            const char *label, jdm_btn_variant_t variant, bool with_arrow)
{
    btn_t *b = lv_malloc(sizeof(btn_t));
    lv_memzero(b, sizeof(*b));
    b->ghost = (variant == JDM_BTN_GHOST);

    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, UIS(mx), UIS(my));
    lv_obj_set_size(c, UIS(mw), UIS(JDM_CTL_ACTION_H));
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(c, b);

    int32_t w = mw, h = JDM_CTL_ACTION_H, cut = JDM_SPACE_XS;
    if (!b->ghost) {
        // 八边形 = 两块矩形并集(相对切角 cut 的四角三角被让出)
        lv_obj_t *fills[2] = {0};
        const int32_t rx[2] = {0, cut};
        const int32_t ry[2] = {cut, 0};
        const int32_t rw[2] = {w, w - 2 * cut};
        const int32_t rh[2] = {h - 2 * cut, h};
        for (int i = 0; i < 2; i++) {
            lv_obj_t *f = lv_obj_create(c);
            lv_obj_remove_style_all(f);
            lv_obj_set_pos(f, UIS(rx[i]), UIS(ry[i]));
            lv_obj_set_size(f, UIS(rw[i]), UIS(rh[i]));
            lv_obj_set_style_bg_color(f, jdm_color(JDM_ROLE_PANEL), 0);
            lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
            lv_obj_clear_flag(f, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            fills[i] = f;
        }
        b->fill[0] = fills[0];
        b->fill[1] = fills[1];
        // 描边:切角八边形闭合折线(9 点)
        const jdm_pt_t oct[9] = {
            {cut, 0}, {w - cut, 0}, {w, cut}, {w, h - cut}, {w - cut, h},
            {cut, h}, {0, h - cut}, {0, cut}, {cut, 0},
        };
        lv_point_precise_t *pts = lv_malloc(sizeof(lv_point_precise_t) * 9);
        for (int i = 0; i < 9; i++) {
            pts[i].x = UIS(oct[i].x);
            pts[i].y = UIS(oct[i].y);
        }
        lv_obj_t *border = lv_line_create(c);
        lv_line_set_points(border, pts, 9);
        lv_obj_set_pos(border, 0, 0);
        lv_obj_set_style_line_width(border, UIS(2), 0);
        lv_obj_set_style_line_rounded(border, true, 0);
        jdm_color_role_t bc = (variant == JDM_BTN_DANGER) ? JDM_ROLE_RED
                            : (variant == JDM_BTN_PRIMARY) ? JDM_ROLE_AMBER
                            : JDM_ROLE_LINE;
        lv_obj_set_style_line_color(border, jdm_color(bc), 0);
        lv_obj_clear_flag(border, LV_OBJ_FLAG_CLICKABLE);
    }

    if (with_arrow) {
        // 原型 back 箭头:折线 (cx−30,32)→(cx−42,44)→(cx−30,56) + 横杆
        int32_t cx = w / 2;
        static const jdm_pt_t chev[3] = {{-30, 32}, {-42, 44}, {-30, 56}};
        static const jdm_pt_t shaft[2] = {{-42, 44}, {-24, 44}};
        const jdm_pt_t *arrows[2] = {chev, shaft};
        const int an[2] = {3, 2};
        for (int i = 0; i < 2; i++) {
            lv_point_precise_t *pts = lv_malloc(sizeof(lv_point_precise_t) * an[i]);
            for (int k = 0; k < an[i]; k++) {
                pts[k].x = UIS(cx + arrows[i][k].x);
                pts[k].y = UIS(arrows[i][k].y);
            }
            lv_obj_t *ln = lv_line_create(c);
            lv_line_set_points(ln, pts, an[i]);
            lv_obj_set_pos(ln, 0, 0);
            lv_obj_set_style_line_width(ln, UIS(3), 0);
            lv_obj_set_style_line_rounded(ln, true, 0);
            lv_obj_set_style_line_color(ln, jdm_color(JDM_ROLE_TEXT), 0);
            lv_obj_clear_flag(ln, LV_OBJ_FLAG_CLICKABLE);
        }
    }

    // 标签:双行或带箭头用 small,单行用 label;行高 font+6 垂直居中
    char tmp[LABEL_BUF];
    const char *lines[MAX_LINES];
    b->nlabel = dup_lines(label, tmp, lines);
    jdm_font_role_t fr = (b->nlabel > 1 || with_arrow) ? JDM_FONT_SMALL : JDM_FONT_LABEL;
    int32_t font_px = (fr == JDM_FONT_SMALL) ? 22 : 26;
    int32_t lh = font_px + 6;
    int32_t start = (h - ((b->nlabel - 1) * lh + font_px)) / 2 - 2;
    for (int i = 0; i < b->nlabel; i++) {
        lv_obj_t *l = lv_label_create(c);
        lv_obj_set_style_text_font(l, jdm_font(fr), 0);
        lv_obj_set_style_text_color(l, jdm_color(JDM_ROLE_TEXT), 0);
        lv_label_set_text(l, lines[i]);
        lv_obj_align(l, LV_ALIGN_TOP_MID, with_arrow ? UIS(14) : 0, UIS(start + i * lh));
        lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
        b->label[i] = l;
    }

    lv_obj_add_event_cb(c, press_cb, LV_EVENT_ALL, b);
    lv_obj_add_event_cb(c, delete_cb, LV_EVENT_DELETE, b);
    return c;
}

lv_obj_t *jdm_btn_create(lv_obj_t *parent, int32_t mx, int32_t my, int32_t mw,
                         const char *label, jdm_btn_variant_t variant)
{
    return jdm_btn_create_ex(parent, mx, my, mw, label, variant, false);
}

void jdm_btn_set_label(lv_obj_t *btn, const char *label)
{
    btn_t *b = lv_obj_get_user_data(btn);
    if (!b) return;
    char tmp[LABEL_BUF];
    const char *lines[MAX_LINES];
    int n = dup_lines(label, tmp, lines);
    for (int i = 0; i < b->nlabel && i < n; i++) {
        if (strcmp(lv_label_get_text(b->label[i]), lines[i]) != 0) {
            lv_label_set_text(b->label[i], lines[i]);
        }
    }
}

void jdm_btn_set_disabled(lv_obj_t *btn, bool disabled)
{
    lv_obj_set_style_opa(btn, disabled ? JDM_CTL_DISABLED_OPA : LV_OPA_COVER, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    if (!disabled) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
}
