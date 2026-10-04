/* obd_gauge_sim — PC SDL2 host for the obd_brz_gauge LVGL UI.
 *
 * Two LVGL displays side by side in one SDL window:
 *   - left (360x360): the firmware UI, rendered exactly like on the device;
 *   - right (240x360, optional): the simulator control panel (control_panel.c)
 *     with sliders that drive the fake-data engine / obd_data_cache.
 *
 * Mirrors app_main.c's LVGL bring-up (theme → Logo → ui_init → ui_ext_init).
 * The flush callback byte-swaps (LV_COLOR_16_SWAP=1, matching the firmware
 * image arrays) into SDL textures. Mouse acts as the CST816 touch; presses
 * are routed to whichever half they started in.
 *
 * The firmware UI sources are compiled unmodified; all ESP-IDF dependencies
 * live in ../shims. */
#include <SDL.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_ext.h"
#include "ui_home_runtime.h"
#include "cli.h"
#include "fake_data.h"
#include "control_panel.h"
#include "sim_platform.h"
#include "sim_clock.h"
#include "esp_timer.h"                     /* sim_esp_timer_poll() */
#include "esp_random.h"                    /* sim_esp_random_seed() */

#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "bsp_obd_dsp/nvs_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SIM_RES_MAX 480
#define PANEL_RES 240

/* 仪表区渲染分辨率 = CONFIG_OBD_UI_RENDER_RES 的模拟(--ui-res 覆盖,
 * 默认 360 = ui_res.h 的 host 回退);窗口/纹理/flush 全部随之参数化。 */
static int s_ui_res = 360;
#define SIM_RES s_ui_res

enum { REGION_NONE = 0, REGION_GAUGE, REGION_PANEL };

static SDL_Window   *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture  *s_gauge_tex;
static SDL_Texture  *s_panel_tex;
static int           s_scale = 2;
static bool          s_panel_on = true;
static long          s_frame;

/* ---- pointer state (fed by SDL events, or by the injectors) ---- */
typedef struct {
    int  x, y;        /* logical screen coords within the owning display */
    bool pressed;
} sim_pointer_t;

static sim_pointer_t s_mouse;    /* real mouse, window coords / scale */
static sim_pointer_t s_inject;   /* tour/tap override (gauge region only) */
static bool s_inject_active;
static int  s_press_region = REGION_NONE;

/* ---- LVGL flush: byte-swap RGB565 into the SDL texture (drv->user_data) ---- */
static uint16_t s_stage[SIM_RES_MAX * SIM_RES_MAX];

static void sim_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    SDL_Texture *tex = (SDL_Texture *)drv->user_data;
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;

    for (int y = 0; y < h; y++) {
        const uint16_t *src = (const uint16_t *)(color_p + (size_t)y * w);
        uint16_t *dst = s_stage + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint16_t v = src[x];
            dst[x] = (uint16_t)((v << 8) | (v >> 8)); /* undo LV_COLOR_16_SWAP */
        }
    }

    SDL_Rect rect = { area->x1, area->y1, w, h };
    SDL_UpdateTexture(tex, &rect, s_stage, (int)(w * sizeof(uint16_t)));
    lv_disp_flush_ready(drv);
}

/* ---- touch: region-routed mouse (+ injectors on the gauge) ---- */
static void feed_pointer(lv_indev_data_t *data, const sim_pointer_t *p)
{
    data->point.x = p->x;
    data->point.y = p->y;
    data->state = p->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void sim_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    if (s_inject_active) {
        feed_pointer(data, &s_inject); /* injectors always drive the gauge */
        return;
    }
    sim_pointer_t p = { s_mouse.x, s_mouse.y,
                        s_mouse.pressed && s_press_region == REGION_GAUGE };
    feed_pointer(data, &p);
    (void)drv;
}

static void sim_touch_cb_panel(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    sim_pointer_t p = { s_mouse.x - SIM_RES, s_mouse.y,
                        s_mouse.pressed && s_press_region == REGION_PANEL };
    if (p.x < 0) p.x = 0;
    if (p.x >= PANEL_RES) p.x = PANEL_RES - 1;
    if (p.y >= SIM_RES) p.y = SIM_RES - 1;
    feed_pointer(data, &p);
    (void)drv;
}

/* ---- screenshot helpers ---- */

/* Compose the current frame into the SDL backbuffer (gauge + optional panel),
 * using the same destination rects as the main loop. ReadPixels-ready. */
static void sim_render_frame(void)
{
    SDL_Rect gauge_dst = { 0, 0, SIM_RES * s_scale, SIM_RES * s_scale };
    SDL_RenderCopy(s_renderer, s_gauge_tex, NULL, &gauge_dst);
    if (s_panel_on) {
        SDL_Rect panel_dst = { SIM_RES * s_scale, 0, PANEL_RES * s_scale, SIM_RES * s_scale };
        SDL_RenderCopy(s_renderer, s_panel_tex, NULL, &panel_dst);
    }
}

static void sim_save_screenshot(const char *path)
{
    int w = (SIM_RES + (s_panel_on ? PANEL_RES : 0)) * s_scale;
    SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, w, SIM_RES * s_scale, 24,
                                                       SDL_PIXELFORMAT_RGB24);
    if (!shot) return;
    sim_render_frame();
    if (SDL_RenderReadPixels(s_renderer, NULL, SDL_PIXELFORMAT_RGB24,
                             shot->pixels, shot->pitch) == 0) {
        SDL_SaveBMP(shot, path);
        fprintf(stderr, "[sim] screenshot: %s\n", path);
    } else {
        fprintf(stderr, "[sim] RenderReadPixels failed: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(shot);
}

static void sim_ensure_dir(const char *path)
{
    mkdir(path, 0755); /* exists_ok semantics; errors ignored */
}

/* ---- tour injector: alternating swipes with a screenshot after each ---- */
typedef enum { TOUR_WAIT, TOUR_PRESS, TOUR_MOVE, TOUR_RELEASE, TOUR_SHOT } tour_phase_t;

static struct {
    int remaining;
    int direction;         /* +1 = swipe right, -1 = swipe left */
    int phase_ms;
    tour_phase_t phase;
    int shot_idx;
    const sim_opts_t *opts;
} s_tour;

static void sim_tour_start(const sim_opts_t *opts)
{
    s_tour.remaining = opts->tour > 0 ? opts->tour : 0;
    s_tour.direction = -1;
    s_tour.phase = TOUR_WAIT;
    s_tour.phase_ms = 0;
    s_tour.shot_idx = 0;
    s_tour.opts = opts;
    s_inject_active = true;
    sim_ensure_dir(opts->shots_dir);
}

static void sim_tour_tick(int dt_ms, bool *quit)
{
    if (s_tour.remaining <= 0) return;
    s_tour.phase_ms += dt_ms;

    switch (s_tour.phase) {
    case TOUR_WAIT:
        if (s_tour.phase_ms >= 1500) {
            s_tour.phase = TOUR_PRESS;
            s_tour.phase_ms = 0;
            s_inject.pressed = true;
            s_inject.x = s_tour.direction < 0 ? 330 : 30;
            s_inject.y = 180;
        }
        break;
    case TOUR_PRESS:
        if (s_tour.phase_ms >= 60) {
            s_tour.phase = TOUR_MOVE;
            s_tour.phase_ms = 0;
        }
        break;
    case TOUR_MOVE: {
        /* ~120ms swipe: press held, x slides across the screen */
        int from = s_tour.direction < 0 ? 330 : 30;
        int to   = s_tour.direction < 0 ? 30  : 330;
        s_inject.x = from + (to - from) * s_tour.phase_ms / 120;
        if (s_tour.phase_ms >= 120) {
            s_tour.phase = TOUR_RELEASE;
            s_tour.phase_ms = 0;
            s_inject.pressed = false;
        }
        break;
    }
    case TOUR_RELEASE:
        if (s_tour.phase_ms >= 900) {   /* let the page transition finish */
            s_tour.phase = TOUR_SHOT;
            s_tour.phase_ms = 0;
        }
        break;
    case TOUR_SHOT: {
        char path[512];
        snprintf(path, sizeof(path), "%s/tour_%03d.bmp", s_tour.opts->shots_dir, s_tour.shot_idx++);
        sim_save_screenshot(path);
        s_tour.remaining--;
        /* first half: swipe left around the carousel ring; second half: back */
        int done = s_tour.shot_idx;
        int total = s_tour.opts->tour;
        s_tour.direction = (done < (total + 1) / 2) ? -1 : +1;
        s_tour.phase = TOUR_WAIT;
        s_tour.phase_ms = 0;
        if (s_tour.remaining <= 0) {
            s_inject_active = false;
            if (s_tour.opts->frames == 0) *quit = true; /* tour finished → done */
        }
        break;
    }
    }
}

/* ---- scripted tap injector (--tap X,Y,FRAME, gauge region) ---- */
static struct {
    int x, y;
    long frame;
    int state;      /* 0=armed 1=pressing 2=done */
    int hold;
} s_tap;

static void sim_tap_start(const sim_opts_t *opts)
{
    s_tap.x = opts->tap_x;
    s_tap.y = opts->tap_y;
    s_tap.frame = opts->tap_frame;
    s_tap.state = opts->tap_frame > 0 ? 0 : 2;
}

static void sim_tap_tick(void)
{
    if (s_tap.state == 0 && s_frame >= s_tap.frame) {
        s_inject_active = true;
        s_inject.x = s_tap.x;
        s_inject.y = s_tap.y;
        s_inject.pressed = true;
        s_tap.state = 1;
        s_tap.hold = 0;
        fprintf(stderr, "[sim] tap injected at (%d,%d)\n", s_tap.x, s_tap.y);
    } else if (s_tap.state == 1 && ++s_tap.hold >= 10) {
        s_inject.pressed = false;
        s_inject_active = false;
        s_tap.state = 2;
    }
}

/* ---- --no-panel fallback: apply the scenario straight to the cache ---- */
static void engine_apply_tick(lv_timer_t *t)
{
    (void)t;
    fake_values_t v;
    fake_data_compute(100, &v);
    obd_data_set_rpm((uint16_t)v.rpm);
    obd_data_set_speed((uint8_t)v.speed);
    obd_data_set_gear((int8_t)v.gear);
    obd_data_set_coolant_temp((int16_t)v.coolant);
    if (v.oil_valid) obd_data_set_oil_temp((int16_t)v.oil);
    obd_data_set_intake_temp((int16_t)v.intake);
    obd_data_set_bat_mv((int32_t)v.bat_mv);
    obd_data_set_oil_pressure_x10((int16_t)v.oilp_x10);
    obd_data_set_boost_x10((int16_t)v.boost_x10);
    obd_data_set_brake_temp_x10((int16_t)v.brake_x10);
    obd_data_set_tps((int16_t)v.tps);
    obd_data_set_load_pct((int16_t)v.load);
    obd_data_set_afr_x100((int16_t)v.afr_x100);
    obd_data_set_brake_rs485_status(v.brake_ok ? BRAKE_RS485_OK : BRAKE_RS485_IDLE);
}

int main(int argc, char **argv)
{
    sim_opts_t opts;
    sim_opts_defaults(&opts);
    if (!sim_opts_parse(&opts, argc, argv)) {
        sim_opts_print_help(argv[0]);
        return 1;
    }
    s_scale = opts.scale;
    s_panel_on = !opts.no_panel;
    if (opts.ui_res >= 120 && opts.ui_res <= SIM_RES_MAX) {
        s_ui_res = opts.ui_res;
    } else if (opts.ui_res != 0) {
        fprintf(stderr, "--ui-res out of range (120-%d): %d\n", SIM_RES_MAX, opts.ui_res);
        return 1;
    }
    if (opts.seed != 0) sim_esp_random_seed((uint64_t)opts.seed); /* determinism */
    sim_clock_set_virtual(opts.virtual_clock);
    if (opts.virtual_clock) sim_esp_timer_use_clock(sim_clock_us);

    SDL_SetMainReady(); /* plain main() on macOS, not SDL_main */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int win_w = (SIM_RES + (s_panel_on ? PANEL_RES : 0)) * s_scale;
    int win_h = SIM_RES * s_scale;
    char title[128];
    snprintf(title, sizeof(title), "obd_gauge_sim (gauge 360x360%s @ %dx)",
             s_panel_on ? " + panel" : "", s_scale);
    s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                win_w, win_h, SDL_WINDOW_SHOWN);
    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);
    if (!s_renderer) s_renderer = SDL_CreateRenderer(s_window, -1, 0); /* software (dummy driver) */
    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }
    s_gauge_tex = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGB565,
                                    SDL_TEXTUREACCESS_STREAMING, SIM_RES, SIM_RES);
    s_panel_tex = s_panel_on
        ? SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGB565,
                            SDL_TEXTUREACCESS_STREAMING, PANEL_RES, SIM_RES)
        : NULL;
    if (!s_gauge_tex || (s_panel_on && !s_panel_tex)) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return 1;
    }

    /* ---- configure the shim layer before anything reads it ---- */
    sim_nvs_mock_configure(&opts);
    sim_bsp_apply_opts(&opts);
    sim_bootmedia_set_dir(opts.bootmedia);
    sim_theme_partition_load(opts.theme);   /* before ui_init() → theme_engine_init() */

    /* ---- LVGL bring-up, mirroring app_main.c ---- */
    lv_init();

    static lv_disp_draw_buf_t gauge_buf;
    static lv_color_t gauge_fb[SIM_RES_MAX * SIM_RES_MAX];   /* full-frame buffer; PC RAM is cheap */
    lv_disp_draw_buf_init(&gauge_buf, gauge_fb, NULL, SIM_RES * SIM_RES);

    static lv_disp_drv_t gauge_drv;
    lv_disp_drv_init(&gauge_drv);
    gauge_drv.hor_res = SIM_RES;
    gauge_drv.ver_res = SIM_RES;
    gauge_drv.flush_cb = sim_flush_cb;
    gauge_drv.draw_buf = &gauge_buf;
    gauge_drv.user_data = s_gauge_tex;
    lv_disp_t *gauge_disp = lv_disp_drv_register(&gauge_drv);

    /* app_main.c step 7: default theme before any screen exists */
    lv_theme_t *theme = lv_theme_default_init(gauge_disp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              false, LV_FONT_DEFAULT);
    lv_disp_set_theme(gauge_disp, theme);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = gauge_disp;
    indev_drv.read_cb = sim_touch_cb;
    lv_indev_drv_register(&indev_drv);

    /* ---- panel display (second LVGL display, same window) ---- */
    static lv_disp_draw_buf_t panel_buf;
    static lv_color_t panel_fb[PANEL_RES * SIM_RES_MAX];
    static lv_disp_drv_t panel_drv;
    static lv_indev_drv_t panel_indev;
    lv_disp_t *panel_disp = NULL;
    if (s_panel_on) {
        lv_disp_draw_buf_init(&panel_buf, panel_fb, NULL, PANEL_RES * SIM_RES);
        lv_disp_drv_init(&panel_drv);
        panel_drv.hor_res = PANEL_RES;
        panel_drv.ver_res = SIM_RES;
        panel_drv.flush_cb = sim_flush_cb;
        panel_drv.draw_buf = &panel_buf;
        panel_drv.user_data = s_panel_tex;
        panel_disp = lv_disp_drv_register(&panel_drv);
        /* theme is per-display; dark variant so labels read on the dark panel */
        lv_theme_t *panel_theme = lv_theme_default_init(
            panel_disp, lv_palette_main(LV_PALETTE_ORANGE), lv_palette_main(LV_PALETTE_AMBER),
            true, LV_FONT_DEFAULT);
        lv_disp_set_theme(panel_disp, panel_theme);

        lv_indev_drv_init(&panel_indev);
        panel_indev.type = LV_INDEV_TYPE_POINTER;
        panel_indev.disp = panel_disp;
        panel_indev.read_cb = sim_touch_cb_panel;
        lv_indev_drv_register(&panel_indev);
    }

    vehicle_profile_set_active(nvs_cfg_get()->vehicle_profile_idx);

    /* Logo first (so it paints while the theme/system pages are built), then
     * the full UI — same as app_main.c step 7. */
    ui_ScreenPageLogo_screen_init();
    lv_scr_load(ui_ScreenPageLogo);
    for (int i = 0; i < 3; i++) {
        lv_timer_handler();
        SDL_Delay(2);
    }
    ui_init();
    ui_ext_init();

    /* M4: boot enters the home pager via ui_ext (flag kept as a no-op for
     * compat with existing scenario scripts) */
    (void)opts.home;

    /* build the control panel on its own display */
    if (s_panel_on) {
        lv_disp_t *def = lv_disp_get_default();
        lv_disp_set_default(panel_disp);   /* lv_obj_create(NULL) lands on default */
        control_panel_build();
        lv_disp_set_default(def);
    }

    vMileageDataStatisticTask();   /* real odometer timer (esp_timer shim) */

    /* data engine: panel tick drives it when visible, else a plain timer */
    fake_data_init(&opts);
    if (!s_panel_on) {
        lv_timer_create(engine_apply_tick, 100, NULL);
    }

    if (opts.tour > 0) sim_tour_start(&opts);
    sim_tap_start(&opts);

    /* ---- main loop ---- */
    bool quit = false;
    Uint32 last_ms = sim_clock_ms();
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) quit = true;
            else if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN ||
                     ev.type == SDL_MOUSEBUTTONUP) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                s_mouse.x = mx / s_scale;
                s_mouse.y = my / s_scale;
                if (s_mouse.x >= SIM_RES + (s_panel_on ? PANEL_RES : 0))
                    s_mouse.x = SIM_RES + (s_panel_on ? PANEL_RES : 0) - 1;
                if (s_mouse.y >= SIM_RES) s_mouse.y = SIM_RES - 1;
                bool down = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
                s_mouse.pressed = down;
                if (down && s_press_region == REGION_NONE) {
                    s_press_region = (s_mouse.x < SIM_RES || !s_panel_on)
                                         ? REGION_GAUGE : REGION_PANEL;
                } else if (!down) {
                    s_press_region = REGION_NONE;
                }
            }
        }

        Uint32 now_ms = sim_clock_ms();
        Uint32 dt = now_ms - last_ms;
        last_ms = now_ms;

        sim_esp_timer_poll();
        sim_tour_tick((int)dt, &quit);
        sim_tap_tick();

        lv_timer_handler();

        sim_render_frame();
        SDL_RenderPresent(s_renderer);

        s_frame++;
        if (opts.frames > 0 && s_frame >= opts.frames) {
            if (opts.screenshot) {
                sim_save_screenshot(opts.screenshot);
            }
            quit = true;
        }

        sim_clock_tick_frame();                 /* advance the virtual clock */
        SDL_Delay(sim_clock_is_virtual() ? 0 : 5); /* virtual = as fast as it renders */
    }

    fprintf(stderr, "[sim] done after %ld frames\n", s_frame);
    SDL_DestroyTexture(s_gauge_tex);
    if (s_panel_tex) SDL_DestroyTexture(s_panel_tex);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    return 0;
}
