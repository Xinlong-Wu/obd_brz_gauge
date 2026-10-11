// ================================================================
//  jdm_road_scene.c — 路景动效实现
//
//  原型参数(foundations.js road + components.js roadProjection):
//    spacing 30m、每侧 5 物体;far(226,372,scale .14)→ near(110,575,.1?1);
//    x = 226−116×depth(左;右镜像),y = 372+203×depth,
//    scale = .14+.86×depth,opacity = min(1,p/.12,(1−p)/.12)×(.4+.6×depth);
//    裁切圆 r330。相位 = slot/5+(右?0.11:0)+travel/150。
// ================================================================

#include "jdm_road_scene.h"

#include <math.h>
#include <stdlib.h>

#include "core/jdm_geo.h"
#include "core/jdm_theme.h"
#include "core/jdm_tokens.h"

#define OBJ_PER_SIDE 5
#define SPACING_M 30
#define CLIP_R 330

typedef struct {
    lv_obj_t *outline;      // 形状轮廓
    lv_obj_t *detail;       // 根部/窗格细节(line 色)
    lv_obj_t *windows;      // 建筑亮窗(仅建筑)
    lv_point_precise_t *pts[3];
    int n[3];
    bool building;
    bool visible;
} road_obj_t;

static struct {
    lv_obj_t *root;
    road_obj_t objs[2][OBJ_PER_SIDE];
    int32_t travel_cm;      // 累计里程(cm;mod 150m 后入相位)
    int32_t last_ms;
    bool started;
} s;

// ---- 形状定义(局部母版坐标;树/建筑轮廓 + 细节) ----
static const lv_point_precise_t s_tree_outline[] = {
    {-18, -12}, {18, -12}, {8, -30}, {13, -30}, {0, -59}, {-13, -30}, {-8, -30}, {-18, -12},
};
static const lv_point_precise_t s_tree_detail[] = {{-7, 0}, {7, 0}};
static const lv_point_precise_t s_bld_outline[] = {
    {-20, 0}, {-20, -70}, {-7, -70}, {-7, -84}, {20, -84}, {20, 0}, {-20, 0},
};
static const lv_point_precise_t s_bld_detail[][2] = {
    {{-11, -58}, {-5, -58}}, {{5, -58}, {11, -58}},
    {{-11, -43}, {-5, -43}}, {{5, -43}, {11, -43}},
    {{-11, -28}, {-5, -28}}, {{5, -28}, {11, -28}},
    {{-4, 0}, {-4, -12}}, {{-4, -12}, {5, -12}}, {{5, -12}, {5, 0}},
};
static const lv_point_precise_t s_bld_windows[][2] = {
    {{-11, -58}, {-5, -58}}, {{5, -43}, {11, -43}},
};

static void line_setup(lv_obj_t **line, lv_point_precise_t *buf, int n,
                       jdm_color_role_t color)
{
    if (*line == NULL) {
        *line = lv_line_create(s.root);
        lv_line_set_points(*line, buf, n);
        lv_obj_set_style_line_rounded(*line, true, 0);
        lv_obj_set_style_line_color(*line, jdm_color(color), 0);
        lv_obj_set_pos(*line, 0, 0);
    }
    lv_line_set_points(*line, buf, n);
}

static void obj_render(road_obj_t *o, float x, float y, float scale, float opa)
{
    if (opa <= 0.02f) {
        if (o->visible) {
            lv_obj_set_hidden(o->outline, true);
            lv_obj_set_hidden(o->detail, true);
            if (o->windows) lv_obj_set_hidden(o->windows, true);
            o->visible = false;
        }
        return;
    }
    o->visible = true;
    lv_obj_set_hidden(o->outline, false);
    lv_obj_set_hidden(o->detail, false);
    if (o->windows) lv_obj_set_hidden(o->windows, false);

    // 轮廓(缩放平移)
    const lv_point_precise_t *def = o->building ? s_bld_outline : s_tree_outline;
    int n = o->building ? 7 : 8;
    for (int i = 0; i < n; i++) {
        o->pts[0][i].x = UIS((int32_t)(x + def[i].x * scale));
        o->pts[0][i].y = UIS((int32_t)(y + def[i].y * scale));
    }
    line_setup(&o->outline, o->pts[0], n, o->building ? JDM_ROLE_MUTED : JDM_ROLE_GREEN);
    lv_obj_set_style_line_width(o->outline, (int32_t)(UIS(3) * scale) + 1, 0);

    // 细节(树:根部横线;建筑:省略窗格细节,仅保留亮窗)
    if (!o->building) {
        o->pts[1][0] = (lv_point_precise_t){UIS((int32_t)(x - 7 * scale)), UIS((int32_t)y)};
        o->pts[1][1] = (lv_point_precise_t){UIS((int32_t)(x + 7 * scale)), UIS((int32_t)y)};
        line_setup(&o->detail, o->pts[1], 2, JDM_ROLE_LINE);
        lv_obj_set_style_line_width(o->detail, (int32_t)(UIS(2) * scale) + 1, 0);
    }

    uint8_t opa8 = (uint8_t)(opa * 255.0f);
    lv_obj_set_style_opa(o->outline, opa8, 0);
    lv_obj_set_style_opa(o->detail, opa8, 0);
    if (o->windows) lv_obj_set_style_opa(o->windows, opa8, 0);
}

lv_obj_t *jdm_road_scene_create(lv_obj_t *parent)
{
    s.root = lv_obj_create(parent);
    lv_obj_remove_style_all(s.root);
    lv_obj_set_size(s.root, UIS(720), UIS(720));
    lv_obj_set_pos(s.root, 0, 0);
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);
    s.travel_cm = 0;
    s.last_ms = 0;
    s.started = false;

    for (int side = 0; side < 2; side++) {
        for (int slot = 0; slot < OBJ_PER_SIDE; slot++) {
            road_obj_t *o = &s.objs[side][slot];
            o->building = ((slot + side) % 3) == 1;   // 右侧镜像原型 side>0 偏移
            o->visible = false;
            o->outline = NULL;
            o->detail = NULL;
            o->windows = o->building ? NULL : NULL;   // 首帧 line_setup 建
            o->pts[0] = lv_malloc(sizeof(lv_point_precise_t) * 8);
            o->pts[1] = lv_malloc(sizeof(lv_point_precise_t) * 18);
            o->pts[2] = lv_malloc(sizeof(lv_point_precise_t) * 4);
        }
    }
    return s.root;
}

void jdm_road_scene_update(lv_obj_t *scene, int32_t speed_kmh, bool moving, int32_t dt_ms)
{
    (void)scene;
    if (!s.started) {
        s.last_ms = lv_tick_get();
        s.started = true;
        return;
    }
    int32_t now = lv_tick_get();
    int32_t dt = dt_ms ? dt_ms : (now - s.last_ms);
    s.last_ms = now;
    if (dt < 0 || dt > 500) dt = 33;

    if (moving && speed_kmh > 0) {
        // m×100:cm = kmh × dt_ms × 100 / 3600000 = kmh×dt/36000
        s.travel_cm += (int32_t)((int64_t)speed_kmh * dt / 36);
    }
    float travel_m = (s.travel_cm / 100 % (SPACING_M * OBJ_PER_SIDE));

    for (int side = 0; side < 2; side++) {
        for (int slot = 0; slot < OBJ_PER_SIDE; slot++) {
            float phase = (float)slot / OBJ_PER_SIDE + (side ? 0.11f : 0.0f)
                          + travel_m / (SPACING_M * OBJ_PER_SIDE);
            float p = phase - floorf(phase);
            float depth = p * p;
            float left_x = 226.0f - 116.0f * depth;
            float x = (side == 0) ? left_x : 720.0f - left_x;
            float y = 372.0f + 203.0f * depth;
            float scale = 0.14f + 0.86f * depth;
            float opa = fminf(1.0f, fminf(p / 0.12f, (1.0f - p) / 0.12f)) * (0.4f + 0.6f * depth);
            obj_render(&s.objs[side][slot], x, y, scale, opa);
        }
    }
}
