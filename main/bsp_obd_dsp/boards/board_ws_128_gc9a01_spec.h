#pragma once
// WS128 板卡静态规格(Waveshare ESP32-S3-LCD-1.28 非触摸版)。
// GC9A01A 240×240 四线 SPI,无触摸/无 TCA9554/无 ADS1115;主控
// ESP32-S3R2(封装内 2MB Quad PSRAM,需 sdkconfig 选 SPIRAM_MODE_QUAD)。
// 引脚来自 Waveshare wiki(引脚分布表):MOSI=11 CLK=10 CS=9 DC=8 RST=12 BL=40。
#include <stdint.h>

#define BOARD_WS_128_GC9A01_NAME "Waveshare ESP32-S3-LCD-1.28"

// 物理面板分辨率
#define BOARD_WS_128_GC9A01_H_RES 240
#define BOARD_WS_128_GC9A01_V_RES 240
#define BOARD_WS_128_GC9A01_COLOR_BITS 16
#define BOARD_WS_128_GC9A01_DRAW_BUFFER_LINES 40
#define BOARD_WS_128_GC9A01_HAS_TOUCH 0

// UI 虚拟分辨率:export_path/ 的布局、字体、开机动画与主题资产全部按
// 360×360 绝对像素生成,本板渲染仍走 360,由 board_ws_128_scale.c 在
// flush 阶段 3:2 降采样到物理 240(比例固定 360:240)。
#define BOARD_WS_128_GC9A01_UI_RES 360

// 四线 SPI 总线与面板控制引脚
#define BOARD_WS_128_GC9A01_SPI_HOST     SPI2_HOST
#define BOARD_WS_128_GC9A01_LCD_SCLK_HZ  (60 * 1000 * 1000)   // wiki 标称 ≤80MHz,取 60 留余量
#define BOARD_WS_128_GC9A01_LCD_MOSI     GPIO_NUM_11
#define BOARD_WS_128_GC9A01_LCD_CLK      GPIO_NUM_10
#define BOARD_WS_128_GC9A01_LCD_CS       GPIO_NUM_9
#define BOARD_WS_128_GC9A01_LCD_DC       GPIO_NUM_8
#define BOARD_WS_128_GC9A01_LCD_RST      GPIO_NUM_12   // 注意:与 RS485 RX 默认脚冲突,WS128 构建不启用 RS485
#define BOARD_WS_128_GC9A01_LCD_BL       GPIO_NUM_40
#define BOARD_WS_128_GC9A01_LCD_TRANS_QUEUE 10

// 面板安装方向校正(MADCTL MX/MY,esp_lcd_panel_mirror)。GC9A01 出厂
// MADCTL=0 为玻璃原生方向:实测本板原生为左右镜像,故只开 X。
// 若画面仍不对:上下颠倒 → 两个都置 1(180° 旋转);仅上下镜像 → 只留 Y;
// 完全正常 → 两个都置 0。
#define BOARD_WS_128_GC9A01_MIRROR_X 1
#define BOARD_WS_128_GC9A01_MIRROR_Y 0

// LEDC 背光(高电平点亮,非反相;若整机不出画面再核查极性)
#define BOARD_WS_128_GC9A01_BL_FREQ_HZ       5000
#define BOARD_WS_128_GC9A01_BL_RESOLUTION    LEDC_TIMER_13_BIT
#define BOARD_WS_128_GC9A01_BL_MAX_DUTY      8191
