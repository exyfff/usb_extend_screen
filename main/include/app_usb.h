/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @file app_usb.h
 * @brief USB应用层头文件
 * @details 定义USB相关的数据结构、常量和函数声明。
 *          支持Vendor视频、HID触摸、UAC音频等多种USB类。
 * @author Espressif
 * @date 2024
 */

#include <stdint.h>
#include "esp_err.h"
#include "tusb.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* JPEG缓冲区大小（300KB） */
#define JPEG_BUFFER_SIZE (300*1024)

/**
 * @brief 触摸点报告结构
 * @details 包含单个触摸点的所有信息
 */
typedef struct {
    uint8_t press_down;   ///< 按下状态（1=按下，0=释放）
    uint8_t index;        ///< 触摸点索引
    uint16_t x;           ///< X坐标
    uint16_t y;           ///< Y坐标
    uint16_t width;       ///< 触摸区域宽度
    uint16_t height;      ///< 触摸区域高度
} __attribute__((packed)) touch_report_t;

/* 编译时检查：确保触摸点数量配置正确 */
_Static_assert(CONFIG_ESP_LCD_TOUCH_MAX_POINTS == 5, "CONFIG_ESP_LCD_TOUCH_MAX_POINTS must be 5");

/**
 * @brief HID报告结构
 * @details 包含报告ID和触摸数据数组
 */
typedef struct {
    uint32_t report_id;    ///< 报告标识符
    struct {
        touch_report_t data[CONFIG_ESP_LCD_TOUCH_MAX_POINTS];  ///< 触摸点数据数组
        // uint16_t scan_time;  /* 扫描时间（保留） */
        uint8_t cnt;         ///< 有效触摸点数量
    } touch_report;
} __attribute__((packed)) hid_report_t;

/**
 * @brief 初始化TinyUSB设备
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 内存不足
 * @details 初始化USB PHY、TinyUSB栈和所有启用的USB类
 */
esp_err_t app_usb_init(void);

#if CFG_TUD_HID
/**
 * @brief 发送HID键盘报告
 * @param report HID报告数据
 * @details 通过USB HID接口发送按键或触摸数据
 */
void tinyusb_hid_keyboard_report(hid_report_t report);

/**
 * @brief 初始化HID设备
 * @return ESP_OK 成功
 */
esp_err_t app_hid_init(void);
#endif

#if CFG_TUD_VENDOR
/**
 * @brief 初始化Vendor类（视频传输）
 * @return ESP_OK 成功
 */
esp_err_t app_vendor_init(void);
#endif

#if CFG_TUD_AUDIO
/**
 * @brief 初始化UAC音频设备
 * @return ESP_OK 成功
 */
esp_err_t app_uac_init(void);
#endif

#ifdef __cplusplus
}
#endif
