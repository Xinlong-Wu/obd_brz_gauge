#pragma once
/* Simulator CLI options. */
#include <stdbool.h>

typedef struct {
    int          scale;        // window scale factor (default 2 → 720x720 window)
    const char  *theme;        // theme.bin path for the runtime-theme partition (NULL = none)
    const char  *bootmedia;    // dir with boot_block.txt/bin (NULL = <repo>/bootmedia/slot_a)
    const char  *scenario;     // fake-data scenario: "drive" (default) | "idle"
    int          profile;      // vehicle profile index (default 0 = OBD2 Generic)
    int          theme_slot;   // compile-time theme slot in registry.txt (default 0 = default)
    int          role;         // 0=master 1=slave 2=standalone (default 2)
    bool         disconnected; // pretend the ELM327 adapter is not connected
    bool         unbound;      // no saved adapter in NVS → boots to the BLE scan page
    bool         no_boot;      // skip the boot video (intro_enable=0)
    long         frames;       // run N frames then exit (0 = run until window closed)
    const char  *screenshot;   // save a BMP of the final frame to this path
    int          tour;         // inject N alternating swipes, screenshotting after each
    const char  *shots_dir;    // where --tour screenshots go (default "sim_shots")
    int          tap_x;        // --tap X,Y,FRAME: scripted screen tap for headless flows
    int          tap_y;
    long         tap_frame;    // frame number at which the tap fires (0 = off)
} sim_opts_t;

void sim_opts_defaults(sim_opts_t *o);
bool sim_opts_parse(sim_opts_t *o, int argc, char **argv);
void sim_opts_print_help(const char *prog);
