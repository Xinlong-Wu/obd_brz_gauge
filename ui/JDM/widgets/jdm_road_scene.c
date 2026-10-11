// ================================================================
//  jdm_road_scene.c — 路景动效实现
//
//  原型参数(foundations.js road + components.js roadProjection):
//    spacing 30m、每侧 5 物体;far(226,372,scale .14)→ near(110,575,1);
//    x = 226−116×depth(左;右镜像),y = 372+203×depth,
//    scale = .14+.86×depth,opacity = min(1,p/.12,(1−p)/.12)×(.4+.6×depth);
//    裁切圆 r330。相位 = slot/5+(右?0.11:0)+travel/150。
//  形体 = 原型 roadScene silhouette/detail 原样:树冠深绿填充+灰绿
//  描边,建筑塔楼 panel 填充+窗框线稿+2 扇暖色亮窗(方帽)。
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

#define TREE_FILL_N 5       // 树冠三角剖分(扇形,顶点 P0)
#define BLD_FILL_N  4       // 建筑 = 底座 2 + 塔楼 2(共享边不重叠)
#define BLD_FRAME_N 6       // 窗框线段
#define BLD_WIN_N   2       // 亮窗线段

typedef struct {
    lv_obj_t *outline;      // 形状轮廓
    lv_obj_t *detail;       // 树:根部横线;建筑:门轮廓
    lv_point_precise_t *pts_outline;   // 8
    lv_point_precise_t *pts_fill;      // 15(树 5×3;建筑 12)
    lv_point_precise_t *pts_segs;      // 建筑 8 段(6 框 + 2 窗)×2
    lv_point_precise_t *pts_detail;    // 4
    int fill_n;
    bool building;
    bool visible;
    float lx, ly, lscale, lopa;        // 上帧参数(脏检查)
    uint8_t opa8;
} road_obj_t;

static struct {
    lv_obj_t *root;
    road_obj_t objs[2][OBJ_PER_SIDE];
    int32_t travel_cm;      // 累计里程(cm;mod 150m 后入相位)
    int32_t last_ms;
    bool started;
} s;

// ---- 形状定义(局部母版坐标,取自原型 roadScene silhouette/detail) ----
static const lv_point_precise_t s_tree_outline[] = {
    {-18, -12}, {18, -12}, {8, -30}, {13, -30}, {0, -59}, {-13, -30}, {-8, -30}, {-18, -12},
};
// 树冠两层三角:扇形剖分(多边形对 P0 星形)
static const lv_point_precise_t s_tree_fill[TREE_FILL_N][3] = {
    {{-18, -12}, {18, -12}, {8, -30}},
    {{-18, -12}, {8, -30}, {13, -30}},
    {{-18, -12}, {13, -30}, {0, -59}},
    {{-18, -12}, {0, -59}, {-13, -30}},
    {{-18, -12}, {-13, -30}, {-8, -30}},
};
static const lv_point_precise_t s_bld_outline[] = {
    {-20, 0}, {-20, -70}, {-7, -70}, {-7, -84}, {20, -84}, {20, 0}, {-20, 0},
};
static const lv_point_precise_t s_bld_fill[BLD_FILL_N][3] = {
    {{-20, 0}, {20, 0}, {20, -70}},
    {{-20, 0}, {20, -70}, {-20, -70}},
    {{-7, -70}, {20, -70}, {20, -84}},
    {{-7, -70}, {20, -84}, {-7, -84}},
};
static const lv_point_precise_t s_bld_door[] = {{-4, 0}, {-4, -12}, {5, -12}, {5, 0}};
static const lv_point_precise_t s_bld_frames[BLD_FRAME_N][2] = {
    {{-11, -58}, {-5, -58}}, {{5, -58}, {11, -58}},
    {{-11, -43}, {-5, -43}}, {{5, -43}, {11, -43}},
    {{-11, -28}, {-5, -28}}, {{5, -28}, {11, -28}},
};
static const lv_point_precise_t s_bld_windows[BLD_WIN_N][2] = {
    {{-11, -58}, {-5, -58}}, {{5, -43}, {11, -43}},
};

static void line_setup(lv_obj_t **line, lv_point_precise_t *buf, int n,
                       jdm_color_role_t color)
{
    if (*line == NULL) {
        *line = lv_line_create(s.root);
        lv_obj_set_style_line_rounded(*line, true, 0);
        lv_obj_set_pos(*line, 0, 0);
    }
    lv_line_set_points(*line, buf, n);
    lv_obj_set_style_line_color(*line, jdm_color(color), 0);
}

// root 的 DRAW_MAIN:画填充三角 + 建筑窗框/亮窗(方帽,亮窗在框之上)
static void root_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_layer_t *layer = lv_event_get_layer(e);

    for (int side = 0; side < 2; side++) {
        for (int slot = 0; slot < OBJ_PER_SIDE; slot++) {
            road_obj_t *o = &s.objs[side][slot];
            if (!o->visible) continue;

            lv_draw_triangle_dsc_t tri;
            lv_draw_triangle_dsc_init(&tri);
            tri.color = jdm_color(o->building ? JDM_ROLE_PANEL : JDM_ROLE_ROAD_FILL);
            tri.opa = o->opa8;
            for (int t = 0; t < o->fill_n; t++) {
                tri.p[0] = o->pts_fill[t * 3 + 0];
                tri.p[1] = o->pts_fill[t * 3 + 1];
                tri.p[2] = o->pts_fill[t * 3 + 2];
                lv_draw_triangle(layer, &tri);
            }

            if (!o->building) continue;
            int32_t w2 = (int32_t)(UIS(2) * o->lscale) + 1;
            int32_t w3 = (int32_t)(UIS(3) * o->lscale) + 1;

            lv_draw_line_dsc_t seg;
            lv_draw_line_dsc_init(&seg);
            seg.round_start = false;    // 原型方帽
            seg.round_end = false;
            seg.opa = o->opa8;
            for (int k = 0; k < BLD_FRAME_N + BLD_WIN_N; k++) {
                seg.p1 = o->pts_segs[k * 2 + 0];
                seg.p2 = o->pts_segs[k * 2 + 1];
                if (k < BLD_FRAME_N) {
                    seg.color = jdm_color(JDM_ROLE_LINE);
                    seg.width = w2;
                } else {
                    seg.color = jdm_color(JDM_ROLE_ROAD_WINDOW);
                    seg.width = w3;
                }
                lv_draw_line(layer, &seg);
            }
        }
    }
}

static bool obj_render(road_obj_t *o, float x, float y, float scale, float opa)
{
    if (opa <= 0.02f) {
        if (o->visible) {
            o->visible = false;
            return true;
        }
        return false;
    }
    bool changed = !o->visible
                   || x != o->lx || y != o->ly || scale != o->lscale || opa != o->lopa;
    o->visible = true;
    o->lx = x;
    o->ly = y;
    o->lscale = scale;
    o->lopa = opa;
    if (!changed) return false;

    // 轮廓(缩放平移)
    const lv_point_precise_t *def = o->building ? s_bld_outline : s_tree_outline;
    int n = o->building ? 7 : 8;
    for (int i = 0; i < n; i++) {
        o->pts_outline[i].x = UIS((int32_t)(x + def[i].x * scale));
        o->pts_outline[i].y = UIS((int32_t)(y + def[i].y * scale));
    }
    line_setup(&o->outline, o->pts_outline, n,
               o->building ? JDM_ROLE_MUTED : JDM_ROLE_ROAD_TREE);
    lv_obj_set_style_line_width(o->outline, (int32_t)(UIS(26) * scale / 10) + 1, 0);

    // 填充三角(变换后供 root_draw_cb 使用)
    const lv_point_precise_t (*fill)[3] = o->building ? s_bld_fill : s_tree_fill;
    o->fill_n = o->building ? BLD_FILL_N : TREE_FILL_N;
    for (int t = 0; t < o->fill_n; t++) {
        for (int k = 0; k < 3; k++) {
            o->pts_fill[t * 3 + k].x = UIS((int32_t)(x + fill[t][k].x * scale));
            o->pts_fill[t * 3 + k].y = UIS((int32_t)(y + fill[t][k].y * scale));
        }
    }

    // 细节:树 = 根部横线;建筑 = 门轮廓 + 窗框/亮窗线段(回调画)
    if (!o->building) {
        o->pts_detail[0] = (lv_point_precise_t){UIS((int32_t)(x - 7 * scale)), UIS((int32_t)y)};
        o->pts_detail[1] = (lv_point_precise_t){UIS((int32_t)(x + 7 * scale)), UIS((int32_t)y)};
        line_setup(&o->detail, o->pts_detail, 2, JDM_ROLE_LINE);
        lv_obj_set_style_line_width(o->detail, (int32_t)(UIS(2) * scale) + 1, 0);
    } else {
        for (int i = 0; i < 4; i++) {
            o->pts_detail[i].x = UIS((int32_t)(x + s_bld_door[i].x * scale));
            o->pts_detail[i].y = UIS((int32_t)(y + s_bld_door[i].y * scale));
        }
        line_setup(&o->detail, o->pts_detail, 4, JDM_ROLE_LINE);
        lv_obj_set_style_line_width(o->detail, (int32_t)(UIS(2) * scale) + 1, 0);
        for (int k = 0; k < BLD_FRAME_N + BLD_WIN_N; k++) {
            const lv_point_precise_t *seg = (k < BLD_FRAME_N)
                                                ? s_bld_frames[k]
                                                : s_bld_windows[k - BLD_FRAME_N];
            o->pts_segs[k * 2 + 0].x = UIS((int32_t)(x + seg[0].x * scale));
            o->pts_segs[k * 2 + 0].y = UIS((int32_t)(y + seg[0].y * scale));
            o->pts_segs[k * 2 + 1].x = UIS((int32_t)(x + seg[1].x * scale));
            o->pts_segs[k * 2 + 1].y = UIS((int32_t)(y + seg[1].y * scale));
        }
    }

    o->opa8 = (uint8_t)(opa * 255.0f);
    lv_obj_set_style_opa(o->outline, o->opa8, 0);
    lv_obj_set_style_opa(o->detail, o->opa8, 0);
    return true;
}

lv_obj_t *jdm_road_scene_create(lv_obj_t *parent)
{
    s.root = lv_obj_create(parent);
    lv_obj_remove_style_all(s.root);
    lv_obj_set_size(s.root, UIS(720), UIS(720));
    lv_obj_set_pos(s.root, 0, 0);
    lv_obj_clear_flag(s.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s.root, root_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
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
            o->lx = o->ly = o->lscale = o->lopa = -1.0f;
            o->pts_outline = lv_malloc(sizeof(lv_point_precise_t) * 8);
            o->pts_fill = lv_malloc(sizeof(lv_point_precise_t) * 15);
            o->pts_detail = lv_malloc(sizeof(lv_point_precise_t) * 4);
            o->pts_segs = o->building ? lv_malloc(sizeof(lv_point_precise_t) * (BLD_FRAME_N + BLD_WIN_N) * 2)
                                      : NULL;
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

    bool dirty = false;
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
            dirty |= obj_render(&s.objs[side][slot], x, y, scale, opa);
        }
    }
    // 填充/线段画在 root 回调里,几何变了要整体重绘
    if (dirty) lv_obj_invalidate(s.root);
}
