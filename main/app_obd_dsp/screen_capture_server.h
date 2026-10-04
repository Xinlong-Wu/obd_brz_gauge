#pragma once
// ================================================================
//  screen_capture_server.h — WiFi 取图服务器(MJPEG 流 + 下载页)
//
//  独立轻量服务器,与 OTA 服务器(:80)、ESP-NOW、BLE OBD 共存:
//    HTTP  :8080  GET /             控制页(实时流 <img> + 下载按钮)
//                 GET /snapshot.jpg 当前帧 JPEG
//                 GET /screenshot.bmp 当前帧精确色 BMP
//    裸 TCP :8081  multipart/x-mixed-replace MJPEG 流(单客户端)
//  esp_http_server 是单任务模型,流必须走独立 socket 任务,否则流会
//  饿死同服务器的其他请求。
//
//  WiFi 拉起策略:未初始化 → 自开 AP(OBD-Gauge-View-XXXX);已是 STA
//  (ESP-NOW)→ 升 APSTA 加本方 AP;已是 AP/APSTA(OTA 模式)→ 直接
//  共用其热点。两种入口:Kconfig OBD_SCREENSHOT_AUTO_START 开机自启
//  (无触摸板),或进入 OTA 模式页后手动访问。
// ================================================================
#include "esp_err.h"

/** 启动取图服务器(幂等;影子未就绪返回错误)。 */
esp_err_t screen_capture_server_start(void);

/** 停止服务器(关 httpd/流/自有的 WiFi)。 */
void screen_capture_server_stop(void);

/** 是否已启动。 */
bool screen_capture_server_running(void);
