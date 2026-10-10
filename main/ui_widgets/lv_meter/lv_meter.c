/**
 * @file lv_meter.c
 * @brief v8 lv_meter 部件移植(LVGL 9 本地部件)。
 *
 * 源自 LVGL v8.4.0 src/extra/widgets/meter/,公开 API/数据结构不变。
 * 对 v9.6 的适配点:
 *  - 绘制走 lv_event_get_layer() + v9 draw dsc(p1/p2 进 line dsc、
 *    arc 用 center/radius、needle-img 用 lv_draw_image 的 rotation(0.1°)+pivot);
 *  - v8 的 draw_mask 径向遮罩体系已删除,刻度线改为三角函数直接算内外端点,
 *    视觉等价且省去遮罩开销;
 *  - v8 的 LV_EVENT_DRAW_PART_BEGIN/END 分事件本仓库未使用,不移植;
 *  - 图片信息查询 lv_img_decoder_get_info → lv_image_decoder_get_info。
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_meter.h"

#include "lvgl_private.h"   // lv_obj_class_t 定义在私有头(v9 第三方部件标准做法)

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_meter_class

/* v9 的 lv_obj_t 不透明:链表状态经 user_data 挂在对象上 */
typedef struct {
    lv_ll_t scale_ll;
    lv_ll_t indicator_ll;
} lv_meter_meta_t;

static lv_meter_meta_t * meter_meta(lv_obj_t * obj)
{
    return (lv_meter_meta_t *)lv_obj_get_user_data(obj);
}

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_meter_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_meter_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_meter_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void draw_arcs(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area);
static void draw_ticks_and_labels(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area);
static void draw_needles(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area);
static void inv_arc(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t old_value, int32_t new_value);
static void inv_line(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t value);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_meter_class = {
    .constructor_cb = lv_meter_constructor,
    .destructor_cb = lv_meter_destructor,
    .event_cb = lv_meter_event,
    .instance_size = sizeof(lv_obj_t),   // 实例状态在 user_data,实例体就是基类 lv_obj_t
    .base_class = &lv_obj_class,
    .name = "lv_meter",
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_meter_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Add scale
 *====================*/

lv_meter_scale_t * lv_meter_add_scale(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);

    lv_meter_scale_t * scale = lv_ll_ins_head(&meter->scale_ll);
    LV_ASSERT_MALLOC(scale);
    lv_memset(scale, 0, sizeof(lv_meter_scale_t));

    scale->angle_range = 270;
    scale->rotation = 90 + (360 - scale->angle_range) / 2;
    scale->min = 0;
    scale->max = 100;
    scale->tick_cnt = 6;
    scale->tick_length = 8;
    scale->tick_width = 2;
    scale->label_gap = 2;

    return scale;
}

void lv_meter_set_scale_ticks(lv_obj_t * obj, lv_meter_scale_t * scale, uint16_t cnt, uint16_t width, uint16_t len,
                              lv_color_t color)
{
    scale->tick_cnt = cnt;
    scale->tick_width = width;
    scale->tick_length = len;
    scale->tick_color = color;
    lv_obj_invalidate(obj);
}

void lv_meter_set_scale_major_ticks(lv_obj_t * obj, lv_meter_scale_t * scale, uint16_t nth, uint16_t width,
                                    uint16_t len, lv_color_t color, int16_t label_gap)
{
    scale->tick_major_nth = nth;
    scale->tick_major_width = width;
    scale->tick_major_length = len;
    scale->tick_major_color = color;
    scale->label_gap = label_gap;
    lv_obj_invalidate(obj);
}

void lv_meter_set_scale_range(lv_obj_t * obj, lv_meter_scale_t * scale, int32_t min, int32_t max, uint32_t angle_range,
                              uint32_t rotation)
{
    scale->min = min;
    scale->max = max;
    scale->angle_range = angle_range;
    scale->rotation = rotation;
    lv_obj_invalidate(obj);
}

/*=====================
 * Add indicator
 *====================*/

lv_meter_indicator_t * lv_meter_add_needle_line(lv_obj_t * obj, lv_meter_scale_t * scale, uint16_t width,
                                                lv_color_t color, int16_t r_mod)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);
    lv_meter_indicator_t * indic = lv_ll_ins_head(&meter->indicator_ll);
    LV_ASSERT_MALLOC(indic);
    lv_memset(indic, 0, sizeof(lv_meter_indicator_t));
    indic->scale = scale;
    indic->opa = LV_OPA_COVER;

    indic->type = LV_METER_INDICATOR_TYPE_NEEDLE_LINE;
    indic->type_data.needle_line.width = width;
    indic->type_data.needle_line.color = color;
    indic->type_data.needle_line.r_mod = r_mod;
    lv_obj_invalidate(obj);

    return indic;
}

lv_meter_indicator_t * lv_meter_add_needle_img(lv_obj_t * obj, lv_meter_scale_t * scale, const void * src,
                                               int32_t pivot_x, int32_t pivot_y)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);
    lv_meter_indicator_t * indic = lv_ll_ins_head(&meter->indicator_ll);
    LV_ASSERT_MALLOC(indic);
    lv_memset(indic, 0, sizeof(lv_meter_indicator_t));
    indic->scale = scale;
    indic->opa = LV_OPA_COVER;

    indic->type = LV_METER_INDICATOR_TYPE_NEEDLE_IMG;
    indic->type_data.needle_img.src = src;
    indic->type_data.needle_img.pivot.x = (int32_t)pivot_x;
    indic->type_data.needle_img.pivot.y = (int32_t)pivot_y;
    lv_obj_invalidate(obj);

    return indic;
}

lv_meter_indicator_t * lv_meter_add_arc(lv_obj_t * obj, lv_meter_scale_t * scale, uint16_t width, lv_color_t color,
                                        int16_t r_mod)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);
    lv_meter_indicator_t * indic = lv_ll_ins_head(&meter->indicator_ll);
    LV_ASSERT_MALLOC(indic);
    lv_memset(indic, 0, sizeof(lv_meter_indicator_t));
    indic->scale = scale;
    indic->opa = LV_OPA_COVER;

    indic->type = LV_METER_INDICATOR_TYPE_ARC;
    indic->type_data.arc.width = width;
    indic->type_data.arc.color = color;
    indic->type_data.arc.r_mod = r_mod;

    lv_obj_invalidate(obj);
    return indic;
}

lv_meter_indicator_t * lv_meter_add_scale_lines(lv_obj_t * obj, lv_meter_scale_t * scale, lv_color_t color_start,
                                                lv_color_t color_end, bool local, int16_t width_mod)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);
    lv_meter_indicator_t * indic = lv_ll_ins_head(&meter->indicator_ll);
    LV_ASSERT_MALLOC(indic);
    lv_memset(indic, 0, sizeof(lv_meter_indicator_t));
    indic->scale = scale;
    indic->opa = LV_OPA_COVER;

    indic->type = LV_METER_INDICATOR_TYPE_SCALE_LINES;
    indic->type_data.scale_lines.color_start = color_start;
    indic->type_data.scale_lines.color_end = color_end;
    indic->type_data.scale_lines.local_grad = local;
    indic->type_data.scale_lines.width_mod = width_mod;

    lv_obj_invalidate(obj);
    return indic;
}

/*=====================
 * Set indicator value
 *====================*/

void lv_meter_set_indicator_value(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t value)
{
    int32_t old_start = indic->start_value;
    int32_t old_end = indic->end_value;
    indic->start_value = value;
    indic->end_value = value;

    if(indic->type == LV_METER_INDICATOR_TYPE_ARC) {
        inv_arc(obj, indic, old_start, value);
        inv_arc(obj, indic, old_end, value);
    }
    else if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG || indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_LINE) {
        inv_line(obj, indic, old_start);
        inv_line(obj, indic, old_end);
        inv_line(obj, indic, value);
    }
    else {
        lv_obj_invalidate(obj);
    }
}

void lv_meter_set_indicator_start_value(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t value)
{
    int32_t old_value = indic->start_value;
    indic->start_value = value;

    if(indic->type == LV_METER_INDICATOR_TYPE_ARC) {
        inv_arc(obj, indic, old_value, value);
    }
    else if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG || indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_LINE) {
        inv_line(obj, indic, old_value);
        inv_line(obj, indic, value);
    }
    else {
        lv_obj_invalidate(obj);
    }
}

void lv_meter_set_indicator_end_value(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t value)
{
    int32_t old_value = indic->end_value;
    indic->end_value = value;

    if(indic->type == LV_METER_INDICATOR_TYPE_ARC) {
        inv_arc(obj, indic, old_value, value);
    }
    else if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG || indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_LINE) {
        inv_line(obj, indic, old_value);
        inv_line(obj, indic, value);
    }
    else {
        lv_obj_invalidate(obj);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_meter_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    lv_meter_meta_t * meter = lv_malloc_zeroed(sizeof(lv_meter_meta_t));
    LV_ASSERT_MALLOC(meter);
    lv_ll_init(&meter->scale_ll, sizeof(lv_meter_scale_t));
    lv_ll_init(&meter->indicator_ll, sizeof(lv_meter_indicator_t));
    lv_obj_set_user_data(obj, meter);
}

static void lv_meter_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_meter_meta_t * meter = meter_meta(obj);
    lv_ll_clear(&meter->indicator_ll);
    lv_ll_clear(&meter->scale_ll);
    lv_free(meter);
    lv_obj_set_user_data(obj, NULL);
}

static void lv_meter_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res = lv_obj_event_base(MY_CLASS, e);
    if(res != LV_RESULT_OK) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_current_target(e);
    if(code == LV_EVENT_DRAW_MAIN) {
        lv_layer_t * layer = lv_event_get_layer(e);
        lv_area_t scale_area;
        lv_obj_get_content_coords(obj, &scale_area);

        draw_arcs(obj, layer, &scale_area);
        draw_ticks_and_labels(obj, layer, &scale_area);
        draw_needles(obj, layer, &scale_area);

        int32_t r_edge = lv_area_get_width(&scale_area) / 2;
        lv_point_t scale_center;
        scale_center.x = scale_area.x1 + r_edge;
        scale_center.y = scale_area.y1 + r_edge;

        lv_draw_rect_dsc_t mid_dsc;
        lv_draw_rect_dsc_init(&mid_dsc);
        lv_obj_init_draw_rect_dsc(obj, LV_PART_INDICATOR, &mid_dsc);
        int32_t w = lv_obj_get_style_width(obj, LV_PART_INDICATOR) / 2;
        int32_t h = lv_obj_get_style_height(obj, LV_PART_INDICATOR) / 2;
        lv_area_t nm_cord;
        nm_cord.x1 = scale_center.x - w;
        nm_cord.y1 = scale_center.y - h;
        nm_cord.x2 = scale_center.x + w;
        nm_cord.y2 = scale_center.y + h;
        lv_draw_rect(layer, &mid_dsc, &nm_cord);
    }
}

static void draw_arcs(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area)
{
    lv_meter_meta_t * meter = meter_meta(obj);

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.rounded = lv_obj_get_style_arc_rounded(obj, LV_PART_ITEMS);

    int32_t r_out = lv_area_get_width(scale_area) / 2 ;
    lv_point_t scale_center;
    scale_center.x = scale_area->x1 + r_out;
    scale_center.y = scale_area->y1 + r_out;

    lv_opa_t opa_main = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);
    lv_meter_indicator_t * indic;

    LV_LL_READ_BACK(&meter->indicator_ll, indic) {
        if(indic->type != LV_METER_INDICATOR_TYPE_ARC) continue;

        arc_dsc.color = indic->type_data.arc.color;
        arc_dsc.width = indic->type_data.arc.width;
        arc_dsc.opa = indic->opa > LV_OPA_MAX ? opa_main : (opa_main * indic->opa) >> 8;

        lv_meter_scale_t * scale = indic->scale;

        int32_t start_angle = lv_map(indic->start_value, scale->min, scale->max, scale->rotation,
                                     scale->rotation + scale->angle_range);
        int32_t end_angle = lv_map(indic->end_value, scale->min, scale->max, scale->rotation,
                                   scale->rotation + scale->angle_range);

        arc_dsc.center = scale_center;
        arc_dsc.radius = r_out + indic->type_data.arc.r_mod;
        arc_dsc.start_angle = start_angle;
        arc_dsc.end_angle = end_angle;

        lv_draw_arc(layer, &arc_dsc);
    }
}

static void draw_ticks_and_labels(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area)
{
    lv_meter_meta_t * meter = meter_meta(obj);

    lv_point_t p_center;
    int32_t r_edge = LV_MIN(lv_area_get_width(scale_area) / 2, lv_area_get_height(scale_area) / 2);
    p_center.x = scale_area->x1 + r_edge;
    p_center.y = scale_area->y1 + r_edge;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.base.layer = layer;
    lv_obj_init_draw_line_dsc(obj, LV_PART_TICKS, &line_dsc);
    line_dsc.raw_end = 1;

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.base.layer = layer;
    lv_obj_init_draw_label_dsc(obj, LV_PART_TICKS, &label_dsc);
    label_dsc.text_local = 1;

    lv_meter_scale_t * scale;

    LV_LL_READ_BACK(&meter->scale_ll, scale) {
        int32_t r_out = r_edge;
        int32_t r_in_minor = r_out - scale->tick_length;
        int32_t r_in_major = r_out - scale->tick_major_length;

        uint32_t minor_cnt = scale->tick_major_nth ? scale->tick_major_nth - 1 : 0xFFFF;
        uint16_t i;
        for(i = 0; i < scale->tick_cnt; i++) {
            minor_cnt++;
            bool major = false;
            if(minor_cnt == scale->tick_major_nth) {
                minor_cnt = 0;
                major = true;
            }

            int32_t value_of_line = lv_map(i, 0, scale->tick_cnt - 1, scale->min, scale->max);

            lv_color_t line_color = major ? scale->tick_major_color : scale->tick_color;
            lv_color_t line_color_ori = line_color;

            int32_t line_width_ori = major ? scale->tick_major_width : scale->tick_width;
            int32_t line_width = line_width_ori;

            lv_meter_indicator_t * indic;
            LV_LL_READ_BACK(&meter->indicator_ll, indic) {
                if(indic->type != LV_METER_INDICATOR_TYPE_SCALE_LINES) continue;
                if(value_of_line >= indic->start_value && value_of_line <= indic->end_value) {
                    line_width += indic->type_data.scale_lines.width_mod;

                    if(lv_color_eq(indic->type_data.scale_lines.color_start, indic->type_data.scale_lines.color_end)) {
                        line_color = indic->type_data.scale_lines.color_start;
                    }
                    else {
                        lv_opa_t ratio;
                        if(indic->type_data.scale_lines.local_grad) {
                            ratio = lv_map(value_of_line, indic->start_value, indic->end_value, LV_OPA_TRANSP, LV_OPA_COVER);
                        }
                        else {
                            ratio = lv_map(value_of_line, scale->min, scale->max, LV_OPA_TRANSP, LV_OPA_COVER);
                        }
                        line_color = lv_color_mix(indic->type_data.scale_lines.color_end, indic->type_data.scale_lines.color_start, ratio);
                    }
                }
            }

            int32_t angle_upscale = ((i * scale->angle_range) * 10) / (scale->tick_cnt - 1) + scale->rotation * 10;

            line_dsc.color = line_color;
            line_dsc.width = line_width;

            /*v8 用长线 + 径向遮罩裁剪;v9 遮罩体系已删,直接三角函数算内外端点*/
            lv_point_t p_outer;
            p_outer.x = p_center.x + r_out;
            p_outer.y = p_center.y;
            lv_point_transform(&p_outer, angle_upscale, 256, 256, &p_center, false);
            lv_point_t p_inner;
            p_inner.x = p_center.x + (major ? r_in_major : r_in_minor);
            p_inner.y = p_center.y;
            lv_point_transform(&p_inner, angle_upscale, 256, 256, &p_center, false);

            line_dsc.p1 = lv_point_to_precise(&p_outer);
            line_dsc.p2 = lv_point_to_precise(&p_inner);
            lv_draw_line(layer, &line_dsc);

            /*Draw the text*/
            if(major) {
                uint32_t r_text = r_in_major - scale->label_gap;
                lv_point_t p;
                p.x = p_center.x + r_text;
                p.y = p_center.y;
                lv_point_transform(&p, angle_upscale, 256, 256, &p_center, false);

                char buf[16];
                lv_snprintf(buf, sizeof(buf), "%" LV_PRId32, value_of_line);

                lv_point_t label_size;
                lv_txt_get_size(&label_size, buf, label_dsc.font, label_dsc.letter_space,
                                label_dsc.line_space,
                                LV_COORD_MAX, LV_TEXT_FLAG_NONE);

                lv_area_t label_cord;
                label_cord.x1 = p.x - label_size.x / 2;
                label_cord.y1 = p.y - label_size.y / 2;
                label_cord.x2 = label_cord.x1 + label_size.x;
                label_cord.y2 = label_cord.y1 + label_size.y;

                label_dsc.text = buf;
                lv_draw_label(layer, &label_dsc, &label_cord);
            }

            line_dsc.color = line_color_ori;
            line_dsc.width = line_width_ori;

        }
    }
}

static void draw_needles(lv_obj_t * obj, lv_layer_t * layer, const lv_area_t * scale_area)
{
    lv_meter_meta_t * meter = meter_meta(obj);

    int32_t r_edge = lv_area_get_width(scale_area) / 2;
    lv_point_t scale_center;
    scale_center.x = scale_area->x1 + r_edge;
    scale_center.y = scale_area->y1 + r_edge;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.base.layer = layer;
    lv_obj_init_draw_line_dsc(obj, LV_PART_ITEMS, &line_dsc);

    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.base.layer = layer;
    lv_obj_init_draw_image_dsc(obj, LV_PART_ITEMS, &img_dsc);
    lv_opa_t opa_main = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);

    lv_meter_indicator_t * indic;
    LV_LL_READ_BACK(&meter->indicator_ll, indic) {
        lv_meter_scale_t * scale = indic->scale;

        if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_LINE) {
            int32_t angle = lv_map(indic->end_value, scale->min, scale->max, scale->rotation, scale->rotation + scale->angle_range);
            int32_t r_out = r_edge + scale->r_mod + indic->type_data.needle_line.r_mod;
            lv_point_t p_end;
            p_end.y = (lv_trigo_sin(angle) * (r_out)) / LV_TRIGO_SIN_MAX + scale_center.y;
            p_end.x = (lv_trigo_cos(angle) * (r_out)) / LV_TRIGO_SIN_MAX + scale_center.x;
            line_dsc.color = indic->type_data.needle_line.color;
            line_dsc.width = indic->type_data.needle_line.width;
            line_dsc.opa = indic->opa > LV_OPA_MAX ? opa_main : (opa_main * indic->opa) >> 8;
            line_dsc.p1 = lv_point_to_precise(&scale_center);
            line_dsc.p2 = lv_point_to_precise(&p_end);
            lv_draw_line(layer, &line_dsc);
        }
        else if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG) {
            if(indic->type_data.needle_img.src == NULL) continue;

            int32_t angle = lv_map(indic->end_value, scale->min, scale->max, scale->rotation, scale->rotation + scale->angle_range);
            lv_image_header_t info;
            lv_image_decoder_get_info(indic->type_data.needle_img.src, &info);
            lv_area_t a;
            a.x1 = scale_center.x - indic->type_data.needle_img.pivot.x;
            a.y1 = scale_center.y - indic->type_data.needle_img.pivot.y;
            a.x2 = a.x1 + info.w - 1;
            a.y2 = a.y1 + info.h - 1;

            img_dsc.src = indic->type_data.needle_img.src;
            img_dsc.opa = indic->opa > LV_OPA_MAX ? opa_main : (opa_main * indic->opa) >> 8;
            img_dsc.pivot.x = indic->type_data.needle_img.pivot.x;
            img_dsc.pivot.y = indic->type_data.needle_img.pivot.y;
            img_dsc.scale_x = LV_SCALE_NONE;
            img_dsc.scale_y = LV_SCALE_NONE;
            angle = angle * 10;
            if(angle > 3600) angle -= 3600;
            img_dsc.rotation = angle;
            img_dsc.image_area = a;

            lv_draw_image(layer, &img_dsc, &a);
        }
    }
}

static void inv_arc(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t old_value, int32_t new_value)
{
    bool rounded = lv_obj_get_style_arc_rounded(obj, LV_PART_ITEMS);

    lv_area_t scale_area;
    lv_obj_get_content_coords(obj, &scale_area);

    int32_t r_out = lv_area_get_width(&scale_area) / 2;
    lv_point_t scale_center;
    scale_center.x = scale_area.x1 + r_out;
    scale_center.y = scale_area.y1 + r_out;

    r_out += indic->type_data.arc.r_mod;

    lv_meter_scale_t * scale = indic->scale;

    int32_t start_angle = lv_map(old_value, scale->min, scale->max, scale->rotation, scale->angle_range + scale->rotation);
    int32_t end_angle = lv_map(new_value, scale->min, scale->max, scale->rotation, scale->angle_range + scale->rotation);

    lv_area_t a;
    lv_draw_arc_get_area(scale_center.x, scale_center.y, r_out, LV_MIN(start_angle, end_angle), LV_MAX(start_angle,
                                                                                                       end_angle), indic->type_data.arc.width, rounded, &a);
    lv_obj_invalidate_area(obj, &a);
}

static void inv_line(lv_obj_t * obj, lv_meter_indicator_t * indic, int32_t value)
{
    lv_area_t scale_area;
    lv_obj_get_content_coords(obj, &scale_area);

    int32_t r_out = lv_area_get_width(&scale_area) / 2;
    lv_point_t scale_center;
    scale_center.x = scale_area.x1 + r_out;
    scale_center.y = scale_area.y1 + r_out;

    lv_meter_scale_t * scale = indic->scale;

    if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_LINE) {
        int32_t angle = lv_map(value, scale->min, scale->max, scale->rotation, scale->rotation + scale->angle_range);
        r_out += scale->r_mod + indic->type_data.needle_line.r_mod;
        lv_point_t p_end;
        p_end.y = (lv_trigo_sin(angle) * (r_out)) / LV_TRIGO_SIN_MAX + scale_center.y;
        p_end.x = (lv_trigo_cos(angle) * (r_out)) / LV_TRIGO_SIN_MAX + scale_center.x;

        lv_area_t a;
        a.x1 = LV_MIN(scale_center.x, p_end.x) - indic->type_data.needle_line.width - 2;
        a.y1 = LV_MIN(scale_center.y, p_end.y) - indic->type_data.needle_line.width - 2;
        a.x2 = LV_MAX(scale_center.x, p_end.x) + indic->type_data.needle_line.width + 2;
        a.y2 = LV_MAX(scale_center.y, p_end.y) + indic->type_data.needle_line.width + 2;

        lv_obj_invalidate_area(obj, &a);
    }
    else if(indic->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG) {
        int32_t angle = lv_map(value, scale->min, scale->max, scale->rotation, scale->rotation + scale->angle_range);
        lv_image_header_t info;
        lv_image_decoder_get_info(indic->type_data.needle_img.src, &info);

        /*旋转图片的包围盒:以枢轴到四角的最大距离为半径的圆(保守估计,略多刷无碍)*/
        int32_t dx = LV_MAX(indic->type_data.needle_img.pivot.x, (int32_t)info.w - indic->type_data.needle_img.pivot.x);
        int32_t dy = LV_MAX(indic->type_data.needle_img.pivot.y, (int32_t)info.h - indic->type_data.needle_img.pivot.y);
        int32_t r = lv_sqrt32((uint32_t)(dx * dx + dy * dy));

        lv_point_t pivot_abs;
        pivot_abs.x = scale_center.x - indic->type_data.needle_img.pivot.x;
        pivot_abs.y = scale_center.y - indic->type_data.needle_img.pivot.y;

        lv_area_t a;
        a.x1 = pivot_abs.x - r - 2;
        a.y1 = pivot_abs.y - r - 2;
        a.x2 = pivot_abs.x + (int32_t)info.w + r + 2;
        a.y2 = pivot_abs.y + (int32_t)info.h + r + 2;

        LV_UNUSED(angle);
        lv_obj_invalidate_area(obj, &a);
    }
}
