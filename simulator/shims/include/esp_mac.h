#pragma once
/* Simulator shim: deterministic MAC (goldens must not drift). */
#include <string.h>

#define ESP_MAC_WIFI_STA 0
#define ESP_MAC_WIFI_SOFTAP 1
#define ESP_MAC_BT 2

static inline int esp_read_mac(uint8_t *mac, int type)
{
    (void)type;
    /* 02:42:53:49:4D:01 — locally administered "SIM" prefix (deterministic) */
    static const uint8_t fixed[6] = {0x02, 0x42, 0x53, 0x49, 0x4D, 0x01};
    memcpy(mac, fixed, 6);
    return 0;
}
