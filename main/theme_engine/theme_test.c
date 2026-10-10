#include "theme_interface.h"
#include "export_path/ui_theme.h"
#include "esp_log.h"

#define TAG "theme_test"

/**
 * Theme engine integration self-test.
 * Call after theme_engine_init(); logs the loaded theme's info, colors,
 * assets and protected-page status, and reports pass/fail.
 *
 * @return true if every probe succeeded (info readable, no protected boot
 *         page is themeable); false otherwise. Wired into the host unit
 *         tests (tests/test_theme_engine.c) and usable from ui_init().
 */
bool theme_engine_test(void) {
    bool ok = true;

    ESP_LOGI(TAG, "=== Theme Engine Test ===");

    // Get theme info
    theme_info_t info;
    esp_err_t ret = theme_get_info(&info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  Theme ID: %s", info.id);
        ESP_LOGI(TAG, "  Name: %s", info.name);
        ESP_LOGI(TAG, "  Version: %s", info.version);
        ESP_LOGI(TAG, "  Author: %s", info.author);
    } else {
        ESP_LOGE(TAG, "  Failed to get theme info");
        return false;
    }

    // Test color access
    ESP_LOGI(TAG, "  Colors:");
    for (int i = 0; i < UI_COLOR__COUNT; i++) {
        lv_color_t color = theme_get_color(i);
        ESP_LOGI(TAG, "    Role %d: 0x%02X%02X%02X", i, color.red, color.green, color.blue);
    }

    // Test asset access
    const lv_img_dsc_t *dial = theme_get_asset("dial");
    const lv_img_dsc_t *ring = theme_get_asset("ring");
    ESP_LOGI(TAG, "  Assets:");
    ESP_LOGI(TAG, "    dial: %s", dial ? "available" : "not available");
    ESP_LOGI(TAG, "    ring: %s", ring ? "available" : "not available");

    // Test protected pages: boot pages must NEVER be themeable
    ESP_LOGI(TAG, "  Protected pages:");
    if (theme_has_page("logo")) {
        ESP_LOGE(TAG, "    logo: can theme = YES (ERROR!)");
        ok = false;
    } else {
        ESP_LOGI(TAG, "    logo: can theme = NO (correct)");
    }
    if (theme_has_page("intro")) {
        ESP_LOGE(TAG, "    intro: can theme = YES (ERROR!)");
        ok = false;
    } else {
        ESP_LOGI(TAG, "    intro: can theme = NO (correct)");
    }
    if (theme_has_page("boot_video")) {
        ESP_LOGE(TAG, "    boot_video: can theme = YES (ERROR!)");
        ok = false;
    } else {
        ESP_LOGI(TAG, "    boot_video: can theme = NO (correct)");
    }

    ESP_LOGI(TAG, "=== Theme Engine Test %s ===", ok ? "Complete" : "FAILED");
    return ok;
}
