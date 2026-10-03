/* obd_gauge_sim — PC SDL2 host for the obd_brz_gauge LVGL UI.
 *
 * Mirrors app_main.c's LVGL bring-up (theme → Logo → ui_init → ui_ext_init),
 * replacing only the display/touch transport: LVGL renders into RAM, the
 * flush callback byte-swaps (LV_COLOR_16_SWAP=1, matching the firmware image
 * arrays) into an SDL texture that is presented in an OS window. Mouse acts
 * as the CST816 touch.
 *
 * The firmware UI sources are compiled unmodified; all ESP-IDF dependencies
 * live in ../shims. */
#include <SDL.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_ext.h"
#include "cli.h"
#include "fake_data.h"
#include "sim_platform.h"
#include "esp_timer.h"                     /* sim_esp_timer_poll() */

#include "app_obd_dsp/obd_data_cache.h"
#include "app_obd_dsp/vehicle_profiles.h"
#include "bsp_obd_dsp/nvs_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define SIM_RES 360

static SDL_Window   *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture  *s_texture;
static int           s_scale = 2;
static long          s_frame;

/* ---- pointer state (fed by SDL events, or by the tour injector) ---- */
typedef struct {
    int  x, y;        /* 0..359 screen space */
    bool pressed;
} sim_pointer_t;

static sim_pointer_t s_mouse;    /* real mouse */
static sim_pointer_t s_inject;   /* tour override */
static bool s_inject_active;

/* ---- LVGL flush: byte-swap RGB565 into the SDL texture ---- */
static uint16_t s_stage[SIM_RES * SIM_RES];

static void sim_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
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
    SDL_UpdateTexture(s_texture, &rect, s_stage, (int)(w * sizeof(uint16_t)));
    lv_disp_flush_ready(drv);
}

/* ---- touch: mouse (or tour injector) ---- */
static void sim_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    const sim_pointer_t *p = s_inject_active ? &s_inject : &s_mouse;
    data->point.x = p->x;
    data->point.y = p->y;
    data->state = p->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    (void)drv;
}

/* ---- screenshot helpers ---- */
static void sim_save_screenshot(const char *path)
{
    SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(
        0, SIM_RES * s_scale, SIM_RES * s_scale, 24, SDL_PIXELFORMAT_RGB24);
    if (!shot) return;
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
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
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

/* ---- scripted tap injector (--tap X,Y,FRAME) ---- */
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

int main(int argc, char **argv)
{
    sim_opts_t opts;
    sim_opts_defaults(&opts);
    if (!sim_opts_parse(&opts, argc, argv)) {
        sim_opts_print_help(argv[0]);
        return 1;
    }
    s_scale = opts.scale;

    SDL_SetMainReady(); /* plain main() on macOS, not SDL_main */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    char title[128];
    snprintf(title, sizeof(title), "obd_gauge_sim (360x360 @ %dx)", s_scale);
    s_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                SIM_RES * s_scale, SIM_RES * s_scale, SDL_WINDOW_SHOWN);
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
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING, SIM_RES, SIM_RES);
    if (!s_texture) {
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

    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t framebuf[SIM_RES * SIM_RES];   /* full-frame buffer; PC RAM is cheap */
    lv_disp_draw_buf_init(&disp_buf, framebuf, NULL, SIM_RES * SIM_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SIM_RES;
    disp_drv.ver_res = SIM_RES;
    disp_drv.flush_cb = sim_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* app_main.c step 7: default theme before any screen exists */
    lv_theme_t *theme = lv_theme_default_init(disp, lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              false, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = sim_touch_cb;
    lv_indev_drv_register(&indev_drv);

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

    vMileageDataStatisticTask();   /* real odometer timer (esp_timer shim) */
    fake_data_start(&opts);
    if (opts.tour > 0) sim_tour_start(&opts);
    sim_tap_start(&opts);

    /* ---- main loop ---- */
    bool quit = false;
    Uint32 last_ms = SDL_GetTicks();
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
                if (s_mouse.x >= SIM_RES) s_mouse.x = SIM_RES - 1;
                if (s_mouse.y >= SIM_RES) s_mouse.y = SIM_RES - 1;
                s_mouse.pressed = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
            }
        }

        Uint32 now_ms = SDL_GetTicks();
        Uint32 dt = now_ms - last_ms;
        last_ms = now_ms;

        sim_esp_timer_poll();
        sim_tour_tick((int)dt, &quit);
        sim_tap_tick();

        lv_timer_handler();

        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);

        s_frame++;
        if (opts.frames > 0 && s_frame >= opts.frames) {
            if (opts.screenshot) {
                sim_save_screenshot(opts.screenshot);
            }
            quit = true;
        }

        SDL_Delay(5);
    }

    fprintf(stderr, "[sim] done after %ld frames\n", s_frame);
    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();
    return 0;
}
