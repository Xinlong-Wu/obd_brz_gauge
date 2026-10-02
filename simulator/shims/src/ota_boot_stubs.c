/* Simulator shim: WiFi OTA server (no WiFi on PC) + boot-media file backend.
 *
 * The real boot_block_player.c is compiled unmodified; it reads everything
 * through boot_media_raw_read_manifest/read_bin/manifest_max, so pointing
 * those at the repo's bootmedia/slot_a/ directory plays the REAL animation.
 * The /bootmedia/... paths passed to boot_block_player_set_paths() are log
 * strings only — they never reach the file layer. */
#include "app_obd_dsp/ota_wifi_server.h"
#include "app_obd_dsp/boot_media_mount.h"
#include "sim_platform.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#ifndef SIM_REPO_ROOT
#define SIM_REPO_ROOT "."
#endif

/* ---- WiFi OTA server ---- */
bool ota_wifi_server_start(ota_wifi_info_t *info, ota_wifi_status_cb_t callback)
{
    (void)info; (void)callback;
    fprintf(stderr, "[sim] ota_wifi_server_start -> failed (no WiFi in simulator)\n");
    return false;
}

void ota_wifi_server_stop(void) {}
void ota_wifi_server_release_bt(void) {}
ota_wifi_state_t ota_wifi_server_get_state(void) { return OTA_WIFI_STATE_IDLE; }
bool ota_wifi_server_is_busy(void) { return false; }

/* ---- boot media (file backend) ---- */
static char s_bootmedia_dir[512];

void sim_bootmedia_set_dir(const char *dir)
{
    if (dir && *dir) {
        snprintf(s_bootmedia_dir, sizeof(s_bootmedia_dir), "%s", dir);
    } else {
        snprintf(s_bootmedia_dir, sizeof(s_bootmedia_dir), "%s/bootmedia/slot_a", SIM_REPO_ROOT);
    }
}

static const char *dir_or_default(void)
{
    if (s_bootmedia_dir[0] == '\0') sim_bootmedia_set_dir(NULL);
    return s_bootmedia_dir;
}

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool files_present(void)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/boot_block.txt", dir_or_default());
    if (!file_exists(path)) return false;
    snprintf(path, sizeof(path), "%s/boot_block.bin", dir_or_default());
    return file_exists(path);
}

bool boot_media_mount(void)
{
    if (!files_present()) {
        fprintf(stderr, "[sim] boot_media_mount: no boot_block files in %s\n", dir_or_default());
        return false;
    }
    fprintf(stderr, "[sim] boot_media_mount: serving %s\n", dir_or_default());
    return true;
}

void boot_media_unmount(void) {}

bool boot_media_has_block_video(void) { return files_present(); }

bool boot_media_recover_previous_if_needed(void) { return true; }
bool boot_media_commit_incoming_update(void)     { return false; }
bool boot_media_remove_active_block(void)        { return false; }
bool boot_media_invalidate_manifest(void)        { return false; }
bool boot_media_erase_data_region(uint32_t total_size, uint32_t manifest_size)
{
    (void)total_size; (void)manifest_size;
    return false;
}

size_t boot_media_get_free_space(void) { return 0; }

esp_err_t boot_media_raw_write(uint32_t abs_offset, const uint8_t *data, size_t len,
                               uint32_t manifest_size)
{
    (void)abs_offset; (void)data; (void)len; (void)manifest_size;
    return ESP_ERR_NOT_SUPPORTED; /* OTA upload path, unused in the simulator */
}

size_t boot_media_raw_manifest_max(void)
{
    return 4096; /* firmware: fixed 4KB manifest slot at partition offset 0 */
}

esp_err_t boot_media_raw_read_manifest(uint8_t *buf, size_t size)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/boot_block.txt", dir_or_default());
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;
    memset(buf, 0, size); /* parser scans to buffer end; zero-fill is required */
    size_t n = fread(buf, 1, size, f);
    fclose(f);
    fprintf(stderr, "[sim] raw_read_manifest: %zu bytes from %s\n", n, path);
    return 0;
}

esp_err_t boot_media_raw_read_bin(uint8_t *buf, size_t size, size_t *out_len)
{
    char path[600];
    snprintf(path, sizeof(path), "%s/boot_block.bin", dir_or_default());
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;

    if (!buf || size == 0) {
        /* size probe (player does this first to learn the file size) */
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        if (out_len) *out_len = (size_t)(sz > 0 ? sz : 0);
        fclose(f);
        return 0;
    }

    size_t n = fread(buf, 1, size, f);
    fclose(f);
    if (out_len) *out_len = n;
    fprintf(stderr, "[sim] raw_read_bin: %zu bytes\n", n);
    return 0;
}
