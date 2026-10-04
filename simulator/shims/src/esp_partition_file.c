/* Simulator shim: file-backed "theme_0" pseudo partition for theme_loader.c.
 * The whole theme.bin blob lives in RAM; read() is a bounds-checked memcpy
 * and mmap() hands out direct pointers into the blob. */
#include "esp_partition.h"
#include "sim_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t s_blob_storage[4 * 1024 * 1024]; /* theme.bin is always 4MB */
static size_t  s_blob_size;
static esp_partition_t s_partition;
static uintptr_t s_handle_seq;

void sim_theme_partition_load(const char *theme_bin_path)
{
    s_blob_size = 0;
    if (!theme_bin_path || !*theme_bin_path) return;

    FILE *f = fopen(theme_bin_path, "rb");
    if (!f) {
        fprintf(stderr, "[sim] theme file not found: %s (continuing without)\n", theme_bin_path);
        return;
    }
    size_t n = fread(s_blob_storage, 1, sizeof(s_blob_storage), f);
    fclose(f);
    if (n == 0) {
        fprintf(stderr, "[sim] theme file empty: %s\n", theme_bin_path);
        return;
    }
    s_blob_size = n;
    s_partition.label = "theme_0";
    s_partition.address = 0;
    s_partition.size = (uint32_t)s_blob_size;
    s_partition.erase_size = 4096;
    s_partition.type = ESP_PARTITION_TYPE_DATA;
    s_partition.subtype = ESP_PARTITION_SUBTYPE_DATA_SPIFFS;
    fprintf(stderr, "[sim] theme partition: %s (%zu bytes)\n", theme_bin_path, s_blob_size);
}

const esp_partition_t *esp_partition_find_first(esp_partition_type_t type,
                                                esp_partition_subtype_t subtype,
                                                const char *label)
{
    if (s_blob_size &&
        type == ESP_PARTITION_TYPE_DATA &&
        subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS &&
        label && strcmp(label, "theme_0") == 0) {
        return &s_partition;
    }
    return NULL;
}

esp_err_t esp_partition_read(const esp_partition_t *partition, size_t src_offset,
                             void *dst, size_t size)
{
    if (!partition || partition != &s_partition) return ESP_ERR_INVALID_ARG;
    if (src_offset > s_blob_size || size > s_blob_size - src_offset) return ESP_ERR_INVALID_SIZE;
    memcpy(dst, s_blob_storage + src_offset, size);
    return 0;
}

esp_err_t esp_partition_mmap(const esp_partition_t *partition, uint32_t offset,
                             uint32_t size, esp_partition_memory_t memory,
                             const void **out_memory,
                             esp_partition_mmap_handle_t *out_handle)
{
    (void)memory;
    if (!partition || partition != &s_partition) return ESP_ERR_INVALID_ARG;
    if ((size_t)offset + size > s_blob_size) return ESP_ERR_INVALID_SIZE;
    *out_memory = s_blob_storage + offset;
    *out_handle = ++s_handle_seq;
    return 0;
}

esp_err_t esp_partition_munmap(esp_partition_mmap_handle_t handle)
{
    (void)handle;
    return 0;
}
