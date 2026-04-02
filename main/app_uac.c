/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_uac.c
 * @brief USB音频类(UAC)实现
 * @details 实现USB音频设备的初始化、音频数据接收、音量控制等功能。
 *          负责将主机通过USB传输的音频数据通过I2S输出到编解码器。
 * @author Espressif
 * @date 2024
 */

#include <stdint.h>
#include "app_usb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb_device_uac.h"
#include "bsp/esp-bsp.h"
#include "bsp/bsp_board_extra.h"

static const char* TAG = "app_uac";

/**
 * @brief UAC音频输出回调函数
 * @param buf 音频数据缓冲区
 * @param len 音频数据长度
 * @param arg 用户参数（未使用）
 * @return ESP_OK 成功，其他值表示失败
 * @note 该函数被TinyUSB库在接收到音频数据时调用
 */
static esp_err_t uac_device_output_cb(uint8_t* buf, size_t len, void* arg)
{
    static uint32_t audio_frame_count = 0;
    static int64_t last_audio_time = 0;
    int64_t current_time = esp_timer_get_time();

    audio_frame_count++;

    if (audio_frame_count % 500 == 0) {
        int64_t interval = current_time - last_audio_time;
        ESP_LOGI(TAG, "Audio output: frame %"PRIu32", len=%zu, interval=%lld us",
                 audio_frame_count, len, interval);
        last_audio_time = current_time;
    }

    /* 调试信息：第一次收到音频数据时打印详细信息和前16字节数据 */
    if (audio_frame_count == 1) {
        ESP_LOGI(TAG, "First audio frame received! len=%zu", len);
        /* 打印前16字节原始数据用于调试 */
        for (int i = 0; i < 16 && i < len; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");
    }

    size_t bytes_written = 0;
    esp_err_t ret = bsp_extra_i2s_write(buf, len, &bytes_written, UINT32_MAX);
    if (ret != ESP_OK || bytes_written != len) {
        static uint32_t error_count = 0;
        if (++error_count % 100 == 1) {
            ESP_LOGW(TAG, "i2s write failed, ret=0x%x, written=%zu/%zu (errors=%"PRIu32")", ret, bytes_written, len, error_count);
        }
        return ret == ESP_OK ? ESP_FAIL : ret;
    }
    return ESP_OK;
}

/**
 * @brief UAC音频输入回调函数
 * @param buf 音频输入缓冲区
 * @param len 缓冲区长度
 * @param bytes_read 实际读取的字节数
 * @param arg 用户参数（未使用）
 * @return ESP_OK 成功
 * @note 从I2S读取音频数据并发送给主机
 */
static esp_err_t uac_device_input_cb(uint8_t* buf, size_t len, size_t* bytes_read, void* arg)
{
    if (bsp_extra_i2s_read(buf, len, bytes_read, 0) != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed");
    }
    return ESP_OK;
}

/**
 * @brief UAC静音设置回调函数
 * @param mute 静音状态（1=静音，0=取消静音）
 * @param arg 用户参数（未使用）
 * @note 设置编解码器的静音状态
 */
static void uac_device_set_mute_cb(uint32_t mute, void* arg)
{
    ESP_LOGD(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    bsp_extra_codec_mute_set(mute);
}

/**
 * @brief UAC音量设置回调函数
 * @param volume 音量值（0-255）
 * @param arg 用户参数（未使用）
 * @note 设置编解码器的音量
 */
static void uac_device_set_volume_cb(uint32_t volume, void* arg)
{
    ESP_LOGD(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    bsp_extra_codec_volume_set(volume, NULL);
}

/**
 * @brief 初始化UAC音频设备
 * @return ESP_OK 成功，其他值表示失败
 * @details 初始化编解码器、设置音频格式、启动UAC设备
 *          配置音频参数：采样率、位深度、声道数
 */
esp_err_t app_uac_init(void)
{
    esp_err_t ret;

    /* 第一步：初始化编解码器 */
    ret = bsp_extra_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Codec init failed: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Codec initialized successfully");

    /* 第二步：设置音频格式（采样率、位深度、声道数） */
    ret = bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE, 16, CONFIG_UAC_SPEAKER_CHANNEL_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Codec set format failed: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Codec format set: %dHz, 16bit, %d channels", CONFIG_UAC_SAMPLE_RATE, CONFIG_UAC_SPEAKER_CHANNEL_NUM);

    /* 第三步：初始化UAC设备并注册回调函数 */
    uac_device_config_t config = {
        .skip_phy_init = true,
        .output_cb = uac_device_output_cb,
        .input_cb = uac_device_input_cb,
        .set_mute_cb = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx = NULL,
    };

    ret = uac_device_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UAC device init failed: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "UAC device initialized successfully");

    return ESP_OK;
}
