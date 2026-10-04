// screen_capture_server.c — WiFi 取图服务器(实现见同名头文件)。
// WiFi 拉起仿 ota_wifi_server.c 的分层:未初始化才 esp_wifi_init(记录
// 所有权),ESP-NOW 的 STA 升 APSTA 复用协议栈,OTA 的 AP 直接共用;
// 停止时只回收自有的资源。
#include "sdkconfig.h"

#if CONFIG_OBD_SCREENSHOT

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/sockets.h"

#include "app_obd_dsp/screen_capture.h"
#include "app_obd_dsp/screen_capture_server.h"

static const char *TAG = "capture_srv";

#define VIEW_SSID_PREFIX    "OBD-Gauge-View-"
#define VIEW_PASSWORD       "88888888"   // 与 OTA 热点同规(>=8 字符 WPA2)
#define VIEW_CHANNEL        1
#define VIEW_HTTP_PORT      8080
#define VIEW_STREAM_PORT    8081
#define VIEW_STREAM_FPS     12
#define VIEW_STREAM_BOUNDARY "frame"

static httpd_handle_t s_httpd;
static int s_listen_fd = -1;
static TaskHandle_t s_stream_task;
static volatile int s_client_fd = -1;
static volatile bool s_running;
static bool s_wifi_owned;       // WiFi 栈是我们 esp_wifi_init 的
static bool s_ap_mine;          // AP 接口是我们创建的
static esp_netif_t *s_ap_netif;
static char s_ssid[sizeof(VIEW_SSID_PREFIX) + 5];

// ---------------------------------------------------------------- WiFi
static esp_err_t capture_wifi_start_ap(const char *ssid)
{
    wifi_config_t ap_cfg = { 0 };
    strncpy((char *)ap_cfg.ap.ssid, ssid, sizeof(ap_cfg.ap.ssid) - 1);
    ap_cfg.ap.ssid_len = (uint8_t)strlen(ssid);
    strncpy((char *)ap_cfg.ap.password, VIEW_PASSWORD, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.channel = VIEW_CHANNEL;
    ap_cfg.ap.max_connection = 3;
    ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "ap config failed");
    esp_err_t err = esp_wifi_start();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "AP '%s' started (psk %s, ch %d)", ssid, VIEW_PASSWORD, VIEW_CHANNEL);
        return ESP_OK;
    }
    // OTA 服务器同款容忍:热点可能已在跑
    ESP_LOGW(TAG, "esp_wifi_start: %s (tolerated, AP may already be up)", esp_err_to_name(err));
    return ESP_OK;
}

/** 确保 WiFi 就绪且有本方 AP;返回页面用的 IP 字符串。 */
static esp_err_t capture_wifi_ensure(char *ip_out, size_t ip_len)
{
    wifi_mode_t mode;
    esp_err_t mode_err = esp_wifi_get_mode(&mode);
    if (mode_err == ESP_ERR_WIFI_NOT_INIT) {
        // 完整自起:netif/事件循环可能已被 ESP-NOW/OTA 建过,容错处理
        ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
        if (esp_event_loop_create_default() != ESP_OK) {
            ESP_LOGD(TAG, "default event loop already exists");
        }
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
        s_wifi_owned = true;
        s_ap_netif = esp_netif_create_default_wifi_ap();
        s_ap_mine = true;
        ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage failed");
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "wifi mode failed");
        uint8_t mac[6] = { 0 };
        esp_wifi_get_mac(WIFI_IF_AP, mac);
        snprintf(s_ssid, sizeof(s_ssid), "%s%02X%02X", VIEW_SSID_PREFIX, mac[4], mac[5]);
        ESP_RETURN_ON_ERROR(capture_wifi_start_ap(s_ssid), TAG, "ap start failed");
    } else if (mode_err == ESP_OK && mode == WIFI_MODE_STA) {
        // ESP-NOW 在跑:升 APSTA,共用协议栈(同 OTA 服务器做法),AP 接口自建
        ESP_LOGI(TAG, "wifi already init (STA/ESP-NOW), upgrading to APSTA");
        s_ap_netif = esp_netif_create_default_wifi_ap();
        s_ap_mine = true;
        ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "wifi mode failed");
        uint8_t mac[6] = { 0 };
        esp_wifi_get_mac(WIFI_IF_AP, mac);
        snprintf(s_ssid, sizeof(s_ssid), "%s%02X%02X", VIEW_SSID_PREFIX, mac[4], mac[5]);
        ESP_RETURN_ON_ERROR(capture_wifi_start_ap(s_ssid), TAG, "ap start failed");
    } else if (mode_err == ESP_OK) {
        // OTA 模式的 AP/APSTA 在跑:直接共用其热点
        ESP_LOGI(TAG, "riding existing AP (mode %d), SSID unchanged", mode);
        strncpy(ip_out, "192.168.4.1", ip_len - 1);
        ip_out[ip_len - 1] = '\0';
        return ESP_OK;
    } else {
        return mode_err;
    }

    // 自建 AP:等 DHCP 出默认地址(192.168.4.1),拿实际 IP 给页面。
    // STA 侧未关联时 modem sleep 会让射频打盹(AP 也无响应),必须关省电
    // (OTA 服务器同款)。
    esp_wifi_set_ps(WIFI_PS_NONE);
    ip_out[0] = '\0';
    for (int i = 0; i < 50 && ip_out[0] == '\0'; i++) {
        esp_netif_ip_info_t info;
        if (s_ap_netif != NULL && esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK &&
            info.ip.addr != 0) {
            snprintf(ip_out, ip_len, IPSTR, IP2STR(&info.ip));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (ip_out[0] == '\0') {
        strncpy(ip_out, "192.168.4.1", ip_len - 1);
        ip_out[ip_len - 1] = '\0';
    }
    return ESP_OK;
}

// ---------------------------------------------------------------- 页面
static esp_err_t page_handler(httpd_req_t *req)
{
    char ip[16] = "192.168.4.1";
    if (s_ap_netif != NULL) {
        esp_netif_ip_info_t info;
        if (esp_netif_get_ip_info(s_ap_netif, &info) == ESP_OK && info.ip.addr != 0) {
            snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
        }
    }

    char *html = heap_caps_malloc(1536, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(html != NULL, ESP_ERR_NO_MEM, TAG, "page buf alloc failed");
    snprintf(html, 1536,
             "<!doctype html><html><head><meta charset='utf-8'>"
             "<meta name='viewport' content='width=device-width,initial-scale=1'>"
             "<title>OBD Gauge View</title>"
             "<style>body{background:#111;color:#eee;font-family:sans-serif;text-align:center;margin:0;padding:16px}"
             "img{max-width:96vw;border-radius:50%%}"
             "a.btn{display:inline-block;margin:10px 6px;padding:10px 18px;background:#2266cc;"
             "color:#fff;border-radius:8px;text-decoration:none;font-size:15px}</style></head><body>"
             "<h3>OBD Gauge 实时画面</h3>"
             "<img src='http://%s:%d/stream' alt='stream'>"
             "<div><a class='btn' href='/snapshot.jpg' download='frame.jpg'>下载当前帧 JPG</a>"
             "<a class='btn' href='/screenshot.bmp' download='screenshot.bmp'>下载精确色 BMP</a></div>"
             "<p style='color:#777;font-size:13px'>若画面未刷新,点这里重连流:<a style='color:#8ab4ff' href='http://%s:%d/stream'>stream</a></p>"
             "</body></html>",
             ip, VIEW_STREAM_PORT, ip, VIEW_STREAM_PORT);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    free(html);
    return ESP_OK;
}

static esp_err_t snapshot_handler(httpd_req_t *req)
{
    const size_t cap = (size_t)screen_capture_res() * screen_capture_res() * 2;
    uint8_t *buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "snapshot alloc failed");
    int size = 0;
    esp_err_t err = screen_capture_jpeg(buf, cap, &size);
    if (err != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "encoder busy");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=frame.jpg");
    err = httpd_resp_send(req, (const char *)buf, size);
    free(buf);
    return err;
}

static esp_err_t screenshot_handler(httpd_req_t *req)
{
    const size_t cap = screen_capture_bmp_size();
    uint8_t *buf = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "bmp alloc failed");
    int size = 0;
    esp_err_t err = screen_capture_bmp(buf, cap, &size);
    if (err != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "capture busy");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=screenshot.bmp");
    err = httpd_resp_send(req, (const char *)buf, size);
    free(buf);
    return err;
}

// ---------------------------------------------------------------- MJPEG 流
/** 发完整数据(处理部分写)。 */
static esp_err_t stream_send_all(int fd, const void *data, size_t len)
{
    const char *p = data;
    while (len > 0) {
        int n = send(fd, p, len, 0);
        if (n <= 0) {
            return ESP_FAIL;
        }
        p += n;
        len -= n;
    }
    return ESP_OK;
}

static void stream_task(void *arg)
{
    uint8_t *frame = heap_caps_malloc((size_t)screen_capture_res() * screen_capture_res() * 2,
                                      MALLOC_CAP_SPIRAM);
    if (frame == NULL) {
        ESP_LOGE(TAG, "stream frame buffer alloc failed");
        vTaskDelete(NULL);
        return;
    }

    while (s_running) {
        int c = accept(s_listen_fd, NULL, NULL);
        if (c < 0) {
            if (s_running) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }
        s_client_fd = c;
        int nodelay = 1;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        ESP_LOGI(TAG, "stream client connected");

        static const char hdr[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=" VIEW_STREAM_BOUNDARY "\r\n"
            "Cache-Control: no-cache\r\n"
            "\r\n";
        if (stream_send_all(c, hdr, sizeof(hdr) - 1) != ESP_OK) {
            goto next_client;
        }

        while (s_running) {
            int size = 0;
            if (screen_capture_jpeg(frame,
                                    (size_t)screen_capture_res() * screen_capture_res() * 2,
                                    &size) != ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(100));   // 编码忙/未就绪,稍后再试
                continue;
            }
            char part[96];
            int plen = snprintf(part, sizeof(part),
                                "--" VIEW_STREAM_BOUNDARY "\r\n"
                                "Content-Type: image/jpeg\r\n"
                                "Content-Length: %d\r\n\r\n", size);
            if (stream_send_all(c, part, plen) != ESP_OK ||
                stream_send_all(c, frame, size) != ESP_OK ||
                stream_send_all(c, "\r\n", 2) != ESP_OK) {
                break;   // 客户端断开
            }
            vTaskDelay(pdMS_TO_TICKS(1000 / VIEW_STREAM_FPS));
        }
next_client:
        close(c);
        s_client_fd = -1;
        ESP_LOGI(TAG, "stream client gone");
    }
    free(frame);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------- 生命周期
esp_err_t screen_capture_server_start(void)
{
    if (s_running) {
        return ESP_OK;
    }
    if (!screen_capture_ready()) {
        ESP_LOGW(TAG, "capture shadow not ready, server not started");
        return ESP_ERR_INVALID_STATE;
    }

    char ip[16];
    ESP_RETURN_ON_ERROR(capture_wifi_ensure(ip, sizeof(ip)), TAG, "wifi bring-up failed");

    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.server_port = VIEW_HTTP_PORT;
    httpd_cfg.max_uri_handlers = 4;
    httpd_cfg.stack_size = 8192;
    httpd_cfg.task_priority = 3;      // 低于 LVGL(4)
    httpd_cfg.core_id = 1;
    httpd_cfg.recv_wait_timeout = 5;
    httpd_cfg.send_wait_timeout = 30;
    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &httpd_cfg), TAG, "httpd start failed");

    const httpd_uri_t uris[] = {
        { .uri = "/",              .method = HTTP_GET, .handler = page_handler },
        { .uri = "/snapshot.jpg",  .method = HTTP_GET, .handler = snapshot_handler },
        { .uri = "/screenshot.bmp", .method = HTTP_GET, .handler = screenshot_handler },
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_httpd, &uris[i]), TAG,
                            "register %s failed", uris[i].uri);
    }

    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ESP_RETURN_ON_FALSE(s_listen_fd >= 0, ESP_FAIL, TAG, "stream socket failed");
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(VIEW_STREAM_PORT),
    };
    int reuse = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ESP_RETURN_ON_ERROR(bind(s_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0
                            ? ESP_FAIL : ESP_OK, TAG, "stream bind failed");
    ESP_RETURN_ON_ERROR(listen(s_listen_fd, 1) < 0 ? ESP_FAIL : ESP_OK, TAG,
                        "stream listen failed");

    s_running = true;
    if (xTaskCreate(stream_task, "capture_strm", 6144, NULL, 3, &s_stream_task) != pdPASS) {
        s_running = false;
        close(s_listen_fd);
        s_listen_fd = -1;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "screen capture up: http://%s:%d/ (page) http://%s:%d/stream (MJPEG)",
             ip, VIEW_HTTP_PORT, ip, VIEW_STREAM_PORT);
    return ESP_OK;
}

void screen_capture_server_stop(void)
{
    if (!s_running) {
        return;
    }
    s_running = false;
    if (s_client_fd >= 0) {
        shutdown(s_client_fd, SHUT_RDWR);
        close(s_client_fd);
        s_client_fd = -1;
    }
    if (s_listen_fd >= 0) {
        shutdown(s_listen_fd, SHUT_RDWR);
        close(s_listen_fd);
        s_listen_fd = -1;
    }
    if (s_stream_task != NULL) {
        // 任务在 accept 阻塞,shutdown 后自行退出
        for (int i = 0; i < 20 && eTaskGetState(s_stream_task) != eDeleted; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        s_stream_task = NULL;
    }
    if (s_httpd != NULL) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    if (s_wifi_owned) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_wifi_owned = false;
    }
    if (s_ap_mine && s_ap_netif != NULL) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
        s_ap_mine = false;
    }
    ESP_LOGI(TAG, "screen capture stopped");
}

bool screen_capture_server_running(void)
{
    return s_running;
}

#endif /* CONFIG_OBD_SCREENSHOT */
