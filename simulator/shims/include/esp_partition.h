#pragma once
/* Simulator shim: file-backed pseudo-partition. Only the APIs that the
 * compiled subset uses exist (theme_loader.c): find_first / read / mmap /
 * munmap. The theme.bin blob is loaded with sim_theme_partition_load()
 * (simulator/src/sim_platform.h) BEFORE ui_init() runs. */
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef uint32_t esp_partition_type_t;
typedef uint8_t  esp_partition_subtype_t;

#define ESP_PARTITION_TYPE_DATA  1
/* Same numeric value the firmware's partitions.csv assigns to the bootmedia
 * partition; theme_0 is declared subtype spiffs (=0x82 in IDF headers). */
#define ESP_PARTITION_SUBTYPE_DATA_SPIFFS 0x82

typedef enum {
    ESP_PARTITION_MMAP_DATA = 0,
    ESP_PARTITION_MMAP_INST,
} esp_partition_memory_t;

typedef uintptr_t esp_partition_mmap_handle_t;

typedef struct {
    const char *label;
    uint32_t    address;
    uint32_t    size;
    uint32_t    erase_size;
    esp_partition_type_t     type;
    esp_partition_subtype_t  subtype;
} esp_partition_t;

const esp_partition_t *esp_partition_find_first(esp_partition_type_t type,
                                                esp_partition_subtype_t subtype,
                                                const char *label);
esp_err_t esp_partition_read(const esp_partition_t *partition, size_t src_offset,
                             void *dst, size_t size);
esp_err_t esp_partition_mmap(const esp_partition_t *partition, uint32_t offset,
                             uint32_t size, esp_partition_memory_t memory,
                             const void **out_memory,
                             esp_partition_mmap_handle_t *out_handle);
esp_err_t esp_partition_munmap(esp_partition_mmap_handle_t handle);
