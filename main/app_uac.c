/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "app_usb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "usb_device_uac.h"
#include "bsp/esp-bsp.h"
#include "bsp/bsp_board_extra.h"

static const char* TAG = "app_uac";

static esp_err_t uac_device_output_cb(uint8_t* buf, size_t len, void* arg)
{
    static uint32_t audio_frame_count = 0;
    static int64_t last_audio_time = 0;
    int64_t current_time = esp_timer_get_time();

    audio_frame_count++;

    // 每500帧打印一次调试信息（约5秒@100fps），减少串口负载
    if (audio_frame_count % 500 == 0) {
        int64_t interval = current_time - last_audio_time;
        ESP_LOGI(TAG, "Audio output: frame %"PRIu32", len=%zu, interval=%lld us",
                 audio_frame_count, len, interval);
        last_audio_time = current_time;
    }

    // 第一次收到音频数据时打印详细信息
    if (audio_frame_count == 1) {
        ESP_LOGI(TAG, "First audio frame received! len=%zu", len);
        // 打印前16字节数据用于调试
        for (int i = 0; i < 16 && i < len; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");
    }

    size_t bytes_written = 0;
    esp_err_t ret = bsp_extra_i2s_write(buf, len, &bytes_written, UINT32_MAX);
    if (ret != ESP_OK || bytes_written != len) {
        // 减少错误日志频率，避免串口阻塞
        static uint32_t error_count = 0;
        if (++error_count % 100 == 1) {
            ESP_LOGW(TAG, "i2s write failed, ret=0x%x, written=%zu/%zu (errors=%"PRIu32")", ret, bytes_written, len, error_count);
        }
        return ret == ESP_OK ? ESP_FAIL : ret;
    }
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t* buf, size_t len, size_t* bytes_read, void* arg)
{
    if (bsp_extra_i2s_read(buf, len, bytes_read, 0) != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed");
    }
    return ESP_OK;
}

static void uac_device_set_mute_cb(uint32_t mute, void* arg)
{
    ESP_LOGD(TAG, "uac_device_set_mute_cb: %"PRIu32"", mute);
    bsp_extra_codec_mute_set(mute);
}

static void uac_device_set_volume_cb(uint32_t volume, void* arg)
{
    ESP_LOGD(TAG, "uac_device_set_volume_cb: %"PRIu32"", volume);
    bsp_extra_codec_volume_set(volume, NULL);
}

esp_err_t app_uac_init(void)
{
    esp_err_t ret;

    // 初始化编解码器
    ret = bsp_extra_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Codec init failed: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Codec initialized successfully");

    // 设置音频格式
    ret = bsp_extra_codec_set_fs(CONFIG_UAC_SAMPLE_RATE, 16, CONFIG_UAC_SPEAKER_CHANNEL_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Codec set format failed: 0x%x", ret);
        return ret;
    }
    ESP_LOGI(TAG, "Codec format set: %dHz, 16bit, %d channels", CONFIG_UAC_SAMPLE_RATE, CONFIG_UAC_SPEAKER_CHANNEL_NUM);

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
