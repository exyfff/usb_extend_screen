/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_lcd.h
 * @brief LCD显示控制头文件
 * @details 定义LCD相关的常量、宏和函数声明。
 *          支持JPEG解码、双缓冲显示等功能。
 * @author Espressif
 * @date 2024
 */

#ifndef ESP_LCD_H
#define ESP_LCD_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ppa.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD分辨率配置
#define EXAMPLE_LCD_H_RES                   (1024)  ///< 水平分辨率
#define EXAMPLE_LCD_V_RES                   (608)   ///< 垂直分辨率

// LCD缓冲区数量（支持双缓冲或三缓冲）
#define EXAMPLE_LCD_BUF_NUM                 (CONFIG_EXAMPLE_LCD_BUF_COUNT)

// 根据像素格式计算每像素位数
#if CONFIG_LCD_PIXEL_FORMAT_RGB565
#define EXAMPLE_LCD_BIT_PER_PIXEL           (16)    ///< RGB565：16位/像素
#elif CONFIG_LCD_PIXEL_FORMAT_RGB888
#define EXAMPLE_LCD_BIT_PER_PIXEL           (24)    ///< RGB888：24位/像素
#endif

// MIPI DPI像素格式映射
#if EXAMPLE_LCD_BIT_PER_PIXEL == 24
#define EXAMPLE_MIPI_DPI_PX_FORMAT          (LCD_COLOR_PIXEL_FORMAT_RGB888)  ///< 24位RGB
#elif EXAMPLE_LCD_BIT_PER_PIXEL == 18
#define EXAMPLE_MIPI_DPI_PX_FORMAT          (LCD_COLOR_PIXEL_FORMAT_RGB666)  ///< 18位RGB
#elif EXAMPLE_LCD_BIT_PER_PIXEL == 16
#define EXAMPLE_MIPI_DPI_PX_FORMAT          (LCD_COLOR_PIXEL_FORMAT_RGB565)  ///< 16位RGB
#endif

// LCD缓冲区大小（RGB565无需对齐，可直接解码）
#define EXAMPLE_LCD_BUF_LEN                 (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * EXAMPLE_LCD_BIT_PER_PIXEL / 8)

/**
 * @brief 初始化LCD显示器
 * @return ESP_OK 成功，ESP_FAIL 失败
 * @details 初始化JPEG解码引擎、配置LCD参数、获取显示缓冲区
 */
esp_err_t app_lcd_init(void);

/**
 * @brief 绘制图像到LCD
 * @param buf 图像数据缓冲区
 * @param len 数据长度
 * @param width 图像宽度
 * @param height 图像高度
 * @details 解码JPEG数据并显示到LCD，支持性能统计和FPS计算
 */
void app_lcd_draw(uint8_t *buf, uint32_t len, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif
