/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_hid.c
 * @brief USB HID设备实现
 * @details 实现USB HID类设备，用于发送触摸数据给主机。
 *          支持远程唤醒、报告队列管理等功能。
 * @author Espressif
 * @date 2024
 */

#include "app_usb.h"
#include "esp_log.h"
#include "esp_check.h"
#include "tusb.h"
#include "device/usbd.h"
#include "tusb_config.h"
#include "usb_descriptors.h"

static const char *TAG = "tinyusb_hid.h";

/**
 * @brief TinyUSB HID私有数据结构
 */
typedef struct {
    TaskHandle_t task_handle;  ///< HID任务句柄
    QueueHandle_t hid_queue;   ///< HID报告队列
} tinyusb_hid_t;

// HID实例指针
static tinyusb_hid_t *s_tinyusb_hid = NULL;

//--------------------------------------------------------------------+
// HID回调函数
//--------------------------------------------------------------------+
#if CFG_TUD_HID

/**
 * @brief 发送HID报告
 * @param report 要发送的HID报告
 * @details 如果设备处于挂起状态，会触发远程唤醒
 */
void tinyusb_hid_keyboard_report(hid_report_t report)
{
    /* 远程唤醒处理 */
    if (tud_suspended()) {
        /* 如果设备处于挂起状态且主机允许远程唤醒，则唤醒主机 */
        tud_remote_wakeup();
    } else {
        /* 将报告加入队列 */
        xQueueSend(s_tinyusb_hid->hid_queue, &report, 0);
    }
}

/**
 * @brief HID任务处理函数
 * @param arg 任务参数（未使用）
 * @details 从队列读取HID报告并发送给主机
 */
static void tinyusb_hid_task(void *arg)
{
    (void) arg;
    hid_report_t report;
    while (1) {
        /* 从队列接收HID报告 */
        if (xQueueReceive(s_tinyusb_hid->hid_queue, &report, portMAX_DELAY)) {
            /* 远程唤醒处理 */
            if (tud_suspended()) {
                /* 如果设备处于挂起状态，唤醒主机并清空队列 */
                tud_remote_wakeup();
                xQueueReset(s_tinyusb_hid->hid_queue);
            } else {
                /* 发送触摸报告 */
                if (report.report_id == REPORT_ID_TOUCH) {
                    tud_hid_n_report(0, REPORT_ID_TOUCH, &report.touch_report, sizeof(report.touch_report));
                } else {
                    /* 未知报告类型，跳过 */
                    continue;
                }
                /* 等待报告发送完成（超时100ms） */
                if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
                    ESP_LOGW(TAG, "Report not sent");
                }
            }
        }
    }
}
#endif

/**
 * @brief 初始化HID设备
 * @return ESP_OK 成功，其他值表示失败
 * @details 创建HID任务和报告队列
 */
esp_err_t app_hid_init(void)
{
    /* 检查是否已经初始化 */
    if (s_tinyusb_hid) {
        ESP_LOGW(TAG, "tinyusb_hid already initialized");
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    /* 分配HID实例内存 */
    s_tinyusb_hid = calloc(1, sizeof(tinyusb_hid_t));
    ESP_RETURN_ON_FALSE(s_tinyusb_hid, ESP_ERR_NO_MEM, TAG, "calloc failed");
    /* 创建HID报告队列（容量10个报告） */
    s_tinyusb_hid->hid_queue = xQueueCreate(10, sizeof(hid_report_t));
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->hid_queue, ESP_ERR_NO_MEM, fail, TAG, "xQueueCreate failed");

    /* 创建HID处理任务 */
    xTaskCreate(tinyusb_hid_task, "tinyusb_hid_task", 4096, NULL, CONFIG_HID_TASK_PRIORITY, &s_tinyusb_hid->task_handle);
    /* 通知任务开始运行 */
    ESP_GOTO_ON_FALSE(s_tinyusb_hid->task_handle, ESP_ERR_NO_MEM, fail, TAG, "xTaskCreate failed");
    xTaskNotifyGive(s_tinyusb_hid->task_handle);
    return ESP_OK;
fail:
    free(s_tinyusb_hid);
    s_tinyusb_hid = NULL;
    return ret;
}

/**
 * @brief HID报告发送完成回调
 * @param itf 接口号
 * @param report 报告数据
 * @param len 报告长度
 * @details 当报告成功发送给主机时被调用，通知HID任务继续发送
 */
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
    (void) itf;
    (void) len;
    xTaskNotifyGive(s_tinyusb_hid->task_handle);
}

/**
 * @brief 获取HID报告回调
 * @param itf 接口号
 * @param report_id 报告ID
 * @param report_type 报告类型
 * @param buffer 输出缓冲区
 * @param reqlen 请求长度
 * @return 实际返回的长度，0表示STALL
 * @note 目前仅返回最大触摸点数量
 */
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    /* TODO: 待实现其他报告类型 */
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    /* 处理最大触摸点数量查询 */
    switch (report_id) {
    case REPORT_ID_MAX_COUNT: {
        buffer[0] = CONFIG_ESP_LCD_TOUCH_MAX_POINTS;
        return 1;
    }
    default: {
        break;
    }
    }

    return 0;
}

/**
 * @brief 设置HID报告回调
 * @param itf 接口号
 * @param report_id 报告ID
 * @param report_type 报告类型
 * @param buffer 接收缓冲区
 * @param bufsize 数据大小
 * @details 主机设置报告时被调用（如设置LED状态）
 * @note TODO: 根据CAPLOCK、NUMLOCK等设置LED
 */
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    /* TODO: 根据主机命令设置LED状态 */
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;

    switch (report_id) {
    default: {
        break;
    }
    }
}
