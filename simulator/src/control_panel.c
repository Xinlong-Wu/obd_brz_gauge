/* Interactive data-adjustment panel (second LVGL display).
 *
 * One 100ms lv_timer orchestrates the data plane:
 *   - engine ON (default): the fake-data scenario drives every channel; a
 *     channel the user is dragging (or has dragged — manual mask) is skipped
 *     and the user's slider value wins instead;
 *   - engine OFF: every channel follows its slider.
 * Slider values are also written back from the engine, so the panel always
 * shows what is actually in the data cache. */
#include "control_panel.h"
#include "fake_data.h"

#include "app_obd_dsp/obd_data_cache.h"
#include "bsp_obd_dsp/elm327_ble_client.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    CH_RPM, CH_SPEED, CH_CLT, CH_OIL, CH_IAT,
    CH_OILP, CH_BOOST, CH_BAT, CH_AFR, CH_TPS, CH_BRAKE,
    CH_COUNT,
} ch_id_t;

typedef struct {
    const char *name;
    const char *unit;
    int min, max;   /* slider range, cache units */
    int div;        /* readout divisor (0 → 1) */
} ch_meta_t;

static const ch_meta_t META[CH_COUNT] = {
    [CH_RPM]   = { "RPM",   "rpm",  0,    8000,  1    },
    [CH_SPEED] = { "SPD",   "km/h", 0,    240,   1    },
    [CH_CLT]   = { "CLT",   "C",    -40,  130,   1    },
    [CH_OIL]   = { "OIL",   "C",    -20,  150,   1    },
    [CH_IAT]   = { "IAT",   "C",    -20,  60,    1    },
    [CH_OILP]  = { "OILP",  "bar",  0,    200,   10   },
    [CH_BOOST] = { "BST",   "bar",  -10,  30,    10   },
    [CH_BAT]   = { "BAT",   "V",    8000, 16000, 1000 },
    [CH_AFR]   = { "AFR",   "",     800,  2200,  100  },
    [CH_TPS]   = { "TPS",   "%",    0,    100,   1    },
    [CH_BRAKE] = { "BRAKE", "C",    0,    8000,  10   },
};

static struct {
    bool engine_on;
    bool manual[CH_COUNT];     /* user touched this channel; engine skips it */
    bool gear_manual;          /* user picked a gear; engine skips it */
    lv_obj_t *slider[CH_COUNT];
    lv_obj_t *value[CH_COUNT];
    lv_obj_t *gear_roller;
    lv_obj_t *conn_label;
    lv_obj_t *conn_btn;
    lv_obj_t *engine_switch;
} s_pan;

/* ---- channel plumbing ---- */

static void apply_channel(ch_id_t ch, int val)
{
    switch (ch) {
    case CH_RPM:   obd_data_set_rpm((uint16_t)val); break;
    case CH_SPEED: obd_data_set_speed((uint8_t)val); break;
    case CH_CLT:   obd_data_set_coolant_temp((int16_t)val); break;
    case CH_OIL:   obd_data_set_oil_temp((int16_t)val); break;
    case CH_IAT:   obd_data_set_intake_temp((int16_t)val); break;
    case CH_OILP:  obd_data_set_oil_pressure_x10((int16_t)val); break;
    case CH_BOOST: obd_data_set_boost_x10((int16_t)val); break;
    case CH_BAT:   obd_data_set_bat_mv((int32_t)val); break;
    case CH_AFR:   obd_data_set_afr_x100((int16_t)val); break;
    case CH_TPS:   obd_data_set_tps((int16_t)val); break;
    case CH_BRAKE: obd_data_set_brake_temp_x10((int16_t)val); break;
    default: break;
    }
}

/* What the scenario wants for this channel; false = engine says "invalid",
 * leave the channel untouched (e.g. oil temp before warm-up). */
static bool engine_value(ch_id_t ch, const fake_values_t *v, int *out)
{
    switch (ch) {
    case CH_RPM:   *out = v->rpm;       return true;
    case CH_SPEED: *out = v->speed;     return true;
    case CH_CLT:   *out = v->coolant;   return true;
    case CH_OIL:   *out = v->oil;       return v->oil_valid;
    case CH_IAT:   *out = v->intake;    return true;
    case CH_OILP:  *out = v->oilp_x10;  return true;
    case CH_BOOST: *out = v->boost_x10; return true;
    case CH_BAT:   *out = v->bat_mv;    return true;
    case CH_AFR:   *out = v->afr_x100;  return true;
    case CH_TPS:   *out = v->tps;       return true;
    case CH_BRAKE: *out = v->brake_x10; return true;
    default:       return false;
    }
}

static void update_readout(ch_id_t ch, int val)
{
    char buf[24];
    int d = META[ch].div;
    if (d > 1) {
        snprintf(buf, sizeof(buf), "%.1f %s", val / (double)d, META[ch].unit);
    } else {
        snprintf(buf, sizeof(buf), "%d %s", val, META[ch].unit);
    }
    lv_label_set_text(s_pan.value[ch], buf);
}

/* ---- events ---- */

static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    if (!lv_slider_is_dragged(slider)) return;   /* engine write-back, not the user */
    for (int i = 0; i < CH_COUNT; i++) {
        if (s_pan.slider[i] == slider) {
            s_pan.manual[i] = true;
            break;
        }
    }
}

static void gear_event_cb(lv_event_t *e)
{
    (void)e;
    s_pan.gear_manual = lv_roller_get_selected(s_pan.gear_roller) != 0; /* 0 = AUTO */
}

static void engine_switch_cb(lv_event_t *e)
{
    (void)e;
    s_pan.engine_on = lv_obj_has_state(s_pan.engine_switch, LV_STATE_CHECKED);
    if (s_pan.engine_on) {
        /* fresh engine run: hand every channel back to the scenario */
        memset(s_pan.manual, 0, sizeof(s_pan.manual));
        s_pan.gear_manual = false;
        lv_roller_set_selected(s_pan.gear_roller, 0, LV_ANIM_OFF);
    }
}

static void auto_btn_cb(lv_event_t *e)
{
    (void)e;
    memset(s_pan.manual, 0, sizeof(s_pan.manual));
    s_pan.gear_manual = false;
    lv_roller_set_selected(s_pan.gear_roller, 0, LV_ANIM_OFF);
}

static const uint8_t s_fake_adapter_mac[6] = { 0x2C, 0xAB, 0x33, 0x11, 0x22, 0x01 };

static void conn_btn_cb(lv_event_t *e)
{
    (void)e;
    if (elm327_ble_is_connected()) {
        elm327_ble_disconnect();
    } else {
        elm327_ble_connect_by_addr(s_fake_adapter_mac, "SIM-ELM327"); /* 1.2s fake connect */
    }
}

/* ---- UI construction ---- */

static lv_obj_t *build_channel_row(lv_obj_t *parent, ch_id_t ch)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 228, 34);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, META[ch].name);
    lv_obj_set_width(name, 42);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);

    lv_obj_t *slider = lv_slider_create(row);
    lv_slider_set_range(slider, META[ch].min, META[ch].max);
    lv_obj_set_width(slider, 96);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_slider_set_value(slider, META[ch].min, LV_ANIM_OFF);

    lv_obj_t *value = lv_label_create(row);
    lv_obj_set_width(value, 74);
    lv_label_set_text(value, "--");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_14, 0);

    s_pan.slider[ch] = slider;
    s_pan.value[ch] = value;
    return row;
}

/* ---- the orchestrating tick ---- */

static void panel_tick(lv_timer_t *t)
{
    (void)t;
    fake_values_t v;
    fake_data_compute(100, &v);
    obd_data_set_brake_rs485_status(v.brake_ok ? BRAKE_RS485_OK : BRAKE_RS485_IDLE);

    for (int i = 0; i < CH_COUNT; i++) {
        if (s_pan.engine_on && !s_pan.manual[i]) {
            int ev;
            if (!engine_value((ch_id_t)i, &v, &ev)) continue; /* engine: channel invalid, leave it */
            lv_slider_set_value(s_pan.slider[i], (int16_t)ev, LV_ANIM_OFF);
            update_readout((ch_id_t)i, ev);
            apply_channel((ch_id_t)i, ev);
        } else {
            int mv = lv_slider_get_value(s_pan.slider[i]);
            update_readout((ch_id_t)i, mv);
            apply_channel((ch_id_t)i, mv);
        }
    }

    /* gear: engine value unless the user picked a gear (or engine is off) */
    uint16_t sel = lv_roller_get_selected(s_pan.gear_roller);
    int gear = (s_pan.engine_on && sel == 0 && !s_pan.gear_manual)
                   ? v.gear
                   : (int)sel - 1; /* index 1 = N(0), 2..9 = gears 1..8 */
    obd_data_set_gear((int8_t)gear);

    /* connection status line + button label */
    bool conn = elm327_ble_is_connected();
    lv_label_set_text_fmt(s_pan.conn_label, "%s", conn ? "OK" : "--");
    lv_obj_t *lbl = lv_obj_get_child(s_pan.conn_btn, 0);
    if (lbl) lv_label_set_text(lbl, conn ? "DISCONNECT" : "CONNECT");
}

void control_panel_build(void)
{
    memset(&s_pan, 0, sizeof(s_pan));
    s_pan.engine_on = true;

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x181818), 0);

    lv_obj_t *col = lv_obj_create(screen);
    lv_obj_set_size(col, 240, 360);
    lv_obj_set_style_pad_all(col, 6, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(col, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(col, LV_SCROLLBAR_MODE_AUTO);

    /* header: title + engine switch */
    lv_obj_t *hdr = lv_obj_create(col);
    lv_obj_set_size(hdr, 228, 34);
    lv_obj_set_style_pad_all(hdr, 2, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "SIM PANEL");
    lv_obj_set_width(title, 140);
    s_pan.engine_switch = lv_switch_create(hdr);
    lv_obj_add_state(s_pan.engine_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_pan.engine_switch, engine_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* channel rows */
    for (int i = 0; i < CH_COUNT; i++) {
        build_channel_row(col, (ch_id_t)i);
    }

    /* gear row (roller: AUTO / N / 1..8) */
    lv_obj_t *grow = lv_obj_create(col);
    lv_obj_set_size(grow, 228, 44);
    lv_obj_set_style_pad_all(grow, 2, 0);
    lv_obj_set_style_border_width(grow, 0, 0);
    lv_obj_set_style_bg_opa(grow, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(grow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(grow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(grow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);
    lv_obj_t *glabel = lv_label_create(grow);
    lv_label_set_text(glabel, "GEAR");
    lv_obj_set_width(glabel, 42);
    lv_obj_set_style_text_font(glabel, &lv_font_montserrat_14, 0);
    s_pan.gear_roller = lv_roller_create(grow);
    lv_roller_set_options(s_pan.gear_roller, "AUTO\nN\n1\n2\n3\n4\n5\n6\n7\n8", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_pan.gear_roller, 1);
    lv_obj_set_width(s_pan.gear_roller, 70);
    lv_obj_add_event_cb(s_pan.gear_roller, gear_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* AUTO reset + connection row */
    lv_obj_t *btnrow = lv_obj_create(col);
    lv_obj_set_size(btnrow, 228, 40);
    lv_obj_set_style_pad_all(btnrow, 2, 0);
    lv_obj_set_style_border_width(btnrow, 0, 0);
    lv_obj_set_style_bg_opa(btnrow, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(btnrow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btnrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END);

    lv_obj_t *auto_btn = lv_btn_create(btnrow);
    lv_obj_set_size(auto_btn, 70, 30);
    lv_obj_t *auto_lbl = lv_label_create(auto_btn);
    lv_label_set_text(auto_lbl, "AUTO");
    lv_obj_center(auto_lbl);
    lv_obj_add_event_cb(auto_btn, auto_btn_cb, LV_EVENT_CLICKED, NULL);

    s_pan.conn_btn = lv_btn_create(btnrow);
    lv_obj_set_size(s_pan.conn_btn, 90, 30);
    lv_obj_t *conn_lbl = lv_label_create(s_pan.conn_btn);
    lv_label_set_text(conn_lbl, "CONNECT");
    lv_obj_center(conn_lbl);
    lv_obj_add_event_cb(s_pan.conn_btn, conn_btn_cb, LV_EVENT_CLICKED, NULL);

    s_pan.conn_label = lv_label_create(btnrow);
    lv_obj_set_width(s_pan.conn_label, 60);
    lv_label_set_text(s_pan.conn_label, "-");
    lv_obj_set_style_text_font(s_pan.conn_label, &lv_font_montserrat_14, 0);

    lv_scr_load(screen); /* loads onto this screen's own display; auto_del=false */

    lv_timer_create(panel_tick, 100, NULL);
}
