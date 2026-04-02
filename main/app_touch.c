/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_touch.c
 * @brief 触摸屏控制实现
 * @details 实现触摸数据的读取、处理和HID报告发送。
 *          支持多点触控，通过USB HID接口将触摸数据发送给主机。
 * @author Espressif
 * @date 2024
 */

#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_lcd_touch.h"
#include "app_usb.h"
#include "usb_descriptors.h"

static const char *TAG = "app_touch";
// 触摸控制器句柄
static esp_lcd_touch_handle_t tp = NULL;

/**
 * @brief 触摸处理任务
 * @param arg 任务参数（未使用）
 * @details 持续读取触摸数据并通过USB HID发送给主机
 *          采样率：125Hz（8ms间隔），平衡响应速度和CPU负载
 */
static void app_touch_task(void *arg)
{
    // 触摸数据数组
    uint16_t x[5];          ///< X坐标数组
    uint16_t y[5];          ///< Y坐标数组
    uint16_t strength[5];   ///< 触摸强度数组
    uint8_t track_id[5];    ///< 触摸点ID数组
    uint8_t touchpad_cnt = 0;  ///< 有效触摸点数量
    bool send_press = false;   ///< 上次是否按下状态
    while (1) {
        /* 读取触摸数据 */
        esp_lcd_touch_read_data(tp);
        /* 获取触摸坐标和强度 */
        bool touchpad_pressed = esp_lcd_touch_get_coordinates(tp, x, y, strength, track_id, &touchpad_cnt, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
        /* 初始化HID报告 */
        hid_report_t report = {0};
        /* 处理触摸按下事件 */
        if (touchpad_pressed && touchpad_cnt > 0) {
            /* 设置报告ID */
            report.report_id = REPORT_ID_TOUCH;
            /* 遍历所有触摸点 */
            int i = 0;
            for (i = 0; i < touchpad_cnt; i++) {
                /* 填充触摸点数据 */
                report.touch_report.data[i].index = track_id[i];
                report.touch_report.data[i].press_down = 1;
                report.touch_report.data[i].x = x[i];
                report.touch_report.data[i].y = y[i];
                report.touch_report.data[i].width = strength[i];
                report.touch_report.data[i].height = strength[i];
                /* 调试信息：仅在DEBUG级别及以上输出 */
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
                printf("(%d: %d, %d. %d) ", track_id[i], x[i], y[i], strength[i]);
#endif
            }
            /* 调试信息：打印换行 */
#if CONFIG_LOG_DEFAULT_LEVEL >= 4
            printf("\n");
#endif
            ESP_LOGD(TAG, "touchpad cnt: %d\n", touchpad_cnt);
            /* 更新触摸点数量 */
            report.touch_report.cnt = touchpad_cnt;
            /* 通过USB HID发送报告 */
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            /* 标记按下状态 */
            send_press = true;
        /* 处理触摸释放事件 */
        } else if (send_press) {
            send_press = false;
            /* 发送释放报告 */
            report.report_id = REPORT_ID_TOUCH;
#if CFG_TUD_HID
            tinyusb_hid_keyboard_report(report);
#endif
            ESP_LOGD(TAG, "send release %d", touchpad_cnt);
        }
        /* 触摸采样间隔：8ms ≈ 125Hz，平衡响应速度和CPU负载 */
        /* GT911最小稳定间隔约5-8ms */
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

/**
 * @brief 初始化触摸控制器
 * @return ESP_OK 成功
 * @details 创建触摸控制器实例并启动触摸处理任务
 */
esp_err_t app_touch_init(void)
{
    /* 创建触摸控制器实例 */
    bsp_touch_new(NULL, &tp);
    /* 创建触摸处理任务 */
    xTaskCreate(app_touch_task, "app_touch_task", 4096, NULL, CONFIG_TOUCH_TASK_PRIORITY, NULL);
    return ESP_OK;
}
