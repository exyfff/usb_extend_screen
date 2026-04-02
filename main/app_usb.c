/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_usb.c
 * @brief USB设备初始化和管理
 * @details 实现USB PHY初始化、TinyUSB栈初始化、复合设备配置等功能。
 *          支持Vendor（自定义视频）、HID（触摸）、UAC（音频）等多种USB类。
 * @author Espressif
 * @date 2024
 */

#include <stdint.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "usb_descriptors.h"
#include "device/usbd.h"
#include "app_usb.h"

static const char *TAG = "app_usb";

//--------------------------------------------------------------------+
// USB PHY配置
//--------------------------------------------------------------------+

/**
 * @brief 初始化USB PHY
 * @note 配置USB OTG控制器为设备模式
 */
static void usb_phy_init(void)
{
    usb_phy_handle_t phy_hdl;
    /* 配置USB PHY为设备模式 */
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };
    phy_conf.target = USB_PHY_TARGET_INT;
    usb_new_phy(&phy_conf, &phy_hdl);
}

/**
 * @brief TinyUSB设备任务
 * @param arg 任务参数（未使用）
 * @details 持续处理USB事务，必须运行在独立任务中
 */
static void tusb_device_task(void *arg)
{
    while (1) {
        tud_task();
    }
}

/**
 * @brief 初始化USB设备
 * @return ESP_OK 成功，ESP_FAIL 失败
 * @details 初始化USB PHY、TinyUSB栈，并根据配置启用各种USB类
 */
esp_err_t app_usb_init(void)
{
    esp_err_t ret = ESP_OK;

    usb_phy_init();
    bool usb_init = tusb_init();
    if (!usb_init) {
        ESP_LOGE(TAG, "USB Device Stack Init Fail");
        return ESP_FAIL;
    }

#if CFG_TUD_VENDOR
    ret = app_vendor_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_vendor_init failed");
#endif

#if CFG_TUD_HID
    ret = app_hid_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_hid_init failed");
#endif

#if CFG_TUD_AUDIO
    ret =  app_uac_init();
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_FAIL, TAG, "app_uac_init failed");
#endif

    /* 性能优化：USB任务绑定核心1，与transfer_task（核心0）分离，减少竞争 */
    xTaskCreatePinnedToCore(tusb_device_task, "tusb_device_task", 4096, NULL, CONFIG_USB_TASK_PRIORITY, NULL, 1);
    return ret;
}

/************************************************** TinyUSB回调函数 ***********************************************/

/**
 * @brief USB设备连接回调
 * @note 当主机连接设备时被调用
 */
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB Mount");
}

/**
 * @brief USB设备断开回调
 * @note 当主机断开设备时被调用
 */
void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB Un-Mount");
}

/**
 * @brief USB总线挂起回调
 * @param remote_wakeup_en 主机是否允许远程唤醒
 * @note 设备必须在7ms内将总线电流降至2.5mA以下
 */
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    ESP_LOGI(TAG, "USB Suspend");
}

/**
 * @brief USB总线恢复回调
 * @note 当USB总线从挂起状态恢复时被调用
 */
void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB Resume");
}
