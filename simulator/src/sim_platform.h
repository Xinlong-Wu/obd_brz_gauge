#pragma once
/* Simulator-internal cross-module API: implemented in the shims directory,
 * called from src/main.c (and vice versa). */
#include <stdbool.h>
#include "cli.h"

/* nvs_storage_mock.c: seed the in-memory NVS with firmware defaults + CLI
 * overrides. Call once before anything reads nvs_cfg_get(). */
void sim_nvs_mock_configure(const sim_opts_t *opts);

/* ota_boot_stubs.c: point the boot-media file backend at a directory
 * containing boot_block.txt + boot_block.bin. NULL = default
 * $SIM_REPO_ROOT/bootmedia/slot_a. */
void sim_bootmedia_set_dir(const char *dir);

/* bsp_stubs.c: apply CLI flags to the canned BSP state (e.g. --disconnected). */
void sim_bsp_apply_opts(const sim_opts_t *opts);
