#pragma once
// WS128T 板卡静态规格(Waveshare ESP32-S3-Touch-LCD-1.28 触摸版)。
// GC9A01A 240×240 四线 SPI + CST816S 电容触摸(与 QMI8658 共享 I2C);
// 主控 ESP32-S3R2(封装内 2MB Quad PSRAM,需 sdkconfig 选 SPIRAM_MODE_QUAD)。
// 注意:与非触摸版(ESP32-S3-LCD-1.28,LCD_RST=12/BL=40)引脚不同!
// 引脚来自 Waveshare wiki(引脚分布表):MOSI=11 CLK=10 CS=9 DC=8
// RST=14 BL=2;TP_SDA=6 TP_SCL=7 TP_RST=13 TP_INT=5。
#include <stdint.h>

#define BOARD_WS_128T_GC9A01_NAME "Waveshare ESP32-S3-Touch-LCD-1.28"

// 物理面板分辨率
#define BOARD_WS_128T_GC9A01_H_RES 240
#define BOARD_WS_128T_GC9A01_V_RES 240
#define BOARD_WS_128T_GC9A01_COLOR_BITS 16
#define BOARD_WS_128T_GC9A01_DRAW_BUFFER_LINES 40
#define BOARD_WS_128T_GC9A01_HAS_TOUCH 1

// 渲染分辨率由 CONFIG_OBD_UI_RENDER_RES 决定(export_path/ui_res.h,
// 720 母版编译期缩放):=240 原生渲染;≠240 时 app_main 自动启用 ui_scale。

// 四线 SPI 总线与面板控制引脚(RST=14 与 BL=2 均为触摸版特有)
#define BOARD_WS_128T_GC9A01_SPI_HOST     SPI2_HOST
#define BOARD_WS_128T_GC9A01_LCD_SCLK_HZ  (60 * 1000 * 1000)   // wiki 标称 ≤80MHz,取 60 留余量
#define BOARD_WS_128T_GC9A01_LCD_MOSI     GPIO_NUM_11
#define BOARD_WS_128T_GC9A01_LCD_CLK      GPIO_NUM_10
#define BOARD_WS_128T_GC9A01_LCD_CS       GPIO_NUM_9
#define BOARD_WS_128T_GC9A01_LCD_DC       GPIO_NUM_8
#define BOARD_WS_128T_GC9A01_LCD_RST      GPIO_NUM_14   // 触摸版为 14(非触摸版 12);注意 V1 的 RS485 默认脚,本板走 V2_NEW
#define BOARD_WS_128T_GC9A01_LCD_BL       GPIO_NUM_2    // 触摸版为 2(非触摸版 40)
#define BOARD_WS_128T_GC9A01_LCD_TRANS_QUEUE 10

// 触摸 CST816S(与 QMI8658 共享 I2C 总线,驱动内固定 400kHz、地址 0x15)
#define BOARD_WS_128T_TP_SDA  GPIO_NUM_6
#define BOARD_WS_128T_TP_SCL  GPIO_NUM_7
#define BOARD_WS_128T_TP_RST  GPIO_NUM_13
#define BOARD_WS_128T_TP_INT  GPIO_NUM_5

// 面板安装方向校正(MADCTL MX/MY,esp_lcd_panel_mirror)。GC9A01 出厂
// MADCTL=0 为玻璃原生方向:与非触摸版同玻璃,实测原生为左右镜像,故只开 X。
// 若画面仍不对:上下颠倒 → 两个都置 1(180° 旋转);仅上下镜像 → 只留 Y;
// 完全正常 → 两个都置 0。
#define BOARD_WS_128T_GC9A01_MIRROR_X 1
#define BOARD_WS_128T_GC9A01_MIRROR_Y 0

// IPS 面板需要 INVON(0x21)才显示正确极性:组件默认初始化不含反相,
// 不开时屏幕黑底白字反相成白底黑字(帧缓冲/截图是正确的,屏幕是反的)。
#define BOARD_WS_128T_GC9A01_INVERT_COLOR 1

// LEDC 背光(高电平点亮,非反相;若整机不出画面再核查极性)
#define BOARD_WS_128T_GC9A01_BL_FREQ_HZ       5000
#define BOARD_WS_128T_GC9A01_BL_RESOLUTION    LEDC_TIMER_13_BIT
#define BOARD_WS_128T_GC9A01_BL_MAX_DUTY      8191

// 触摸方向:触摸报的是玻璃原生坐标,显示端软件镜像了 X(见上 MIRROR_X),
// 因此触摸也要镜像 X 才与 UI 对齐(esp_lcd_touch 内部做 x = x_max-1-x)。
#define BOARD_WS_128T_TOUCH_SWAP_XY 0
#define BOARD_WS_128T_TOUCH_MIRROR_X 1
#define BOARD_WS_128T_TOUCH_MIRROR_Y 0
