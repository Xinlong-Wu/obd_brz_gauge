/* Simulator CLI parsing. */
#include "cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sim_opts_defaults(sim_opts_t *o)
{
    memset(o, 0, sizeof(*o));
    o->scale = 2;
    o->scenario = "drive";
    o->profile = 0;
    o->theme_slot = 0;
    o->role = 2;          /* standalone */
    o->shots_dir = "sim_shots";
}

void sim_opts_print_help(const char *prog)
{
    printf(
        "obd_gauge_sim — PC SDL simulator for the obd_brz_gauge UI\n"
        "\n"
        "Usage: %s [options]\n"
        "  --scale N          window scale, 360*N pixels (default 2)\n"
        "  --scenario NAME    fake-data scenario: drive (default) | idle\n"
        "  --profile N        vehicle profile index (default 0 = OBD2 Generic)\n"
        "  --theme-slot N     compile-time theme slot in themes/registry.txt (default 0)\n"
        "  --role ROLE        master | slave | standalone (default standalone)\n"
        "  --disconnected     with --bound: show the adapter as not connected\n"
        "  --bound            pre-saved adapter in NVS (boots straight to the gauges)\n"
        "  --no-boot          skip the boot video (intro mode OFF)\n"
        "  --no-panel         hide the side data panel (plain 360x360 window)\n"
        "  --theme FILE       load theme.bin as the runtime-theme partition\n"
        "  --bootmedia DIR    dir with boot_block.txt/bin (default <repo>/bootmedia/slot_a)\n"
        "  --frames N         run N frames then exit (default: run until closed)\n"
        "  --screenshot FILE  save the final frame as BMP\n"
        "  --tour N           inject N alternating swipes, screenshotting after each\n"
        "  --shots-dir DIR    where --tour screenshots go (default sim_shots)\n"
        "  --seed N           pin the fake-data PRNG (deterministic screenshots)\n"
        "  --home             preview the user-dashboard home runtime (M4)\n"
        "  --clock MODE       real (default) | virtual — frame-locked clock for\n"
        "                     bit-identical headless screenshots (implies fast-run)\n"
        "  --tap X,Y,FRAME    scripted tap at screen coords on a given frame (headless)\n"
        "  --help\n"
        "\n"
        "Headless (CI/screenshot) mode: SDL_VIDEODRIVER=dummy ./obd_gauge_sim --frames 300 ...\n",
        prog);
}

static int parse_role(const char *s)
{
    if (strcmp(s, "master") == 0 || strcmp(s, "0") == 0) return 0;
    if (strcmp(s, "slave") == 0 || strcmp(s, "1") == 0) return 1;
    if (strcmp(s, "standalone") == 0 || strcmp(s, "2") == 0) return 2;
    return -1;
}

bool sim_opts_parse(sim_opts_t *o, int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
#define NEXT() ((i + 1 < argc) ? argv[++i] : NULL)
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            sim_opts_print_help(argv[0]);
            exit(0);
        } else if (strcmp(a, "--scale") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->scale = atoi(v);
            if (o->scale < 1) o->scale = 1;
        } else if (strcmp(a, "--scenario") == 0) {
            o->scenario = NEXT();
            if (!o->scenario) return false;
        } else if (strcmp(a, "--profile") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->profile = atoi(v);
        } else if (strcmp(a, "--theme-slot") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->theme_slot = atoi(v);
        } else if (strcmp(a, "--role") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->role = parse_role(v);
            if (o->role < 0) {
                fprintf(stderr, "bad --role value: %s\n", v);
                return false;
            }
        } else if (strcmp(a, "--disconnected") == 0) {
            o->disconnected = true;
        } else if (strcmp(a, "--bound") == 0) {
            o->bound = true;
        } else if (strcmp(a, "--no-boot") == 0) {
            o->no_boot = true;
        } else if (strcmp(a, "--home") == 0) {
            o->home = true;
        } else if (strcmp(a, "--no-panel") == 0) {
            o->no_panel = true;
        } else if (strcmp(a, "--theme") == 0) {
            o->theme = NEXT();
            if (!o->theme) return false;
        } else if (strcmp(a, "--bootmedia") == 0) {
            o->bootmedia = NEXT();
            if (!o->bootmedia) return false;
        } else if (strcmp(a, "--frames") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->frames = atol(v);
        } else if (strcmp(a, "--screenshot") == 0) {
            o->screenshot = NEXT();
            if (!o->screenshot) return false;
        } else if (strcmp(a, "--tour") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->tour = atoi(v);
        } else if (strcmp(a, "--shots-dir") == 0) {
            o->shots_dir = NEXT();
            if (!o->shots_dir) return false;
        } else if (strcmp(a, "--seed") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            o->seed = atol(v);
        } else if (strcmp(a, "--clock") == 0) {
            const char *v = NEXT();
            if (!v) return false;
            if (strcmp(v, "virtual") == 0) o->virtual_clock = true;
            else if (strcmp(v, "real") == 0) o->virtual_clock = false;
            else {
                fprintf(stderr, "bad --clock value: %s (real|virtual)\n", v);
                return false;
            }
        } else if (strcmp(a, "--tap") == 0) {
            const char *v = NEXT();
            if (!v || sscanf(v, "%d,%d,%ld", &o->tap_x, &o->tap_y, &o->tap_frame) != 3) {
                fprintf(stderr, "--tap expects X,Y,FRAME (e.g. --tap 180,170,300)\n");
                return false;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", a);
            return false;
        }
#undef NEXT
    }
    return true;
}
