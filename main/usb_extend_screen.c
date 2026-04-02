/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usb_extend_screen.c
 * @brief USB扩展显示器主程序
 * @details 实现通过USB 2.0高速接口为缺少视频接口的主机扩展显示功能。
 *          支持视频传输、音频播放、触摸输入等功能。
 * @author Espressif
 * @date 2024
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "app_usb.h"
#include "usb_descriptors.h"
#include "esp_log.h"
#include "app_touch.h"
#include "app_lcd.h"

static const char *TAG = "usb_extend_screen";

/**
 * @brief 主函数
 * @details 初始化并启动USB扩展显示器系统的所有功能模块
 *          1. 初始化USB设备（Vendor视频、HID触摸、UAC音频）
 *          2. 初始化LCD显示器
 *          3. 初始化触摸控制器
 */
void app_main(void)
{
    ESP_LOGI(TAG, "启动USB扩展显示器系统");
    app_usb_init();
    app_lcd_init();
    app_touch_init();
}
