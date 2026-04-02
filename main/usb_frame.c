/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usb_frame.c
 * @brief USB视频帧缓冲管理
 * @details 实现帧缓冲区的分配、管理、队列操作等功能。
 *          支持多缓冲区机制，用于USB视频数据的接收和处理。
 * @author Espressif
 * @date 2024
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* 空缓冲区队列句柄 */
static QueueHandle_t empty_fb_queue = NULL;
/* 已填充缓冲区队列句柄 */
static QueueHandle_t filled_fb_queue = NULL;
static const char *TAG = "usb_frame";

/**
 * @brief 分配帧缓冲区
 * @param nb_of_fb 缓冲区数量
 * @param fb_size 每个缓冲区的大小
 * @return ESP_OK 成功，其他值表示失败
 * @details 创建指定数量和大小的帧缓冲区，并初始化队列
 */
esp_err_t frame_allocate(int nb_of_fb, size_t fb_size)
{
    esp_err_t ret = ESP_OK;
    /* JPEG内存分配配置：用于输入缓冲区 */
    jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER,
    };

    /* We will be passing the frame buffers by reference */
    /* 创建空缓冲区队列 */
    empty_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    ESP_RETURN_ON_FALSE(empty_fb_queue, ESP_ERR_NO_MEM, TAG, "Not enough memory for empty_fb_queue %d", nb_of_fb);
    /* 创建已填充缓冲区队列 */
    filled_fb_queue = xQueueCreate(nb_of_fb, sizeof(frame_t *));
    ESP_RETURN_ON_FALSE(filled_fb_queue, ESP_ERR_NO_MEM, TAG, "Not enough memory for filled_fb_queue %d", nb_of_fb);
    for (int i = 0; i < nb_of_fb; i++) {
        /* Allocate the frame buffer */
        frame_t *this_fb = calloc(1, sizeof(frame_t));
        ESP_RETURN_ON_FALSE(this_fb, ESP_ERR_NO_MEM,  TAG, "Not enough memory for frame buffers %d", fb_size);
        size_t malloc_size = 0;
        uint8_t *this_data = (uint8_t*)jpeg_alloc_decoder_mem(fb_size, &tx_mem_cfg, &malloc_size);
        if (!this_data) {
            free(this_fb);
            ret = ESP_ERR_NO_MEM;
            ESP_LOGE(TAG, "Not enough memory for frame buffers %d", fb_size);
            break;
        }

        /* Set members to default */
        this_fb->data = this_data;
        this_fb->data_buffer_len = fb_size;
        this_fb->data_len = 0;

        /* Add the frame to Queue of empty frames */
        const BaseType_t result = xQueueSend(empty_fb_queue, &this_fb, portMAX_DELAY);
        if (result != pdPASS) {
            free(this_fb);
            ret = ESP_FAIL;
            ESP_LOGE(TAG, "Failed to enqueue frame buffer");
            break;
        }
    }
    return ret;
}

/**
 * @brief 重置帧状态
 * @param frame 要重置的帧
 * @details 清除数据长度，将帧标记为空
 */
void frame_reset(frame_t *frame)
{
    assert(frame);
    frame->data_len = 0;
}

/**
 * @brief 将帧返回到空缓冲区队列
 * @param frame 要返回的帧
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t frame_return_empty(frame_t *frame)
{
    frame_reset(frame);
    BaseType_t result = xQueueSend(empty_fb_queue, &frame, portMAX_DELAY);
    ESP_RETURN_ON_FALSE(result == pdPASS, ESP_ERR_NO_MEM, TAG, "Not enough memory empty_fb_queue");
    return ESP_OK;
}

/**
 * @brief 将帧发送到已填充队列
 * @param frame 要发送的帧
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 队列已满
 * @note 如果队列已满，会丢弃当前帧
 */
esp_err_t frame_send_filled(frame_t *frame)
{
    frame_reset(frame);
    BaseType_t result = xQueueSend(filled_fb_queue, &frame, 0);
    if (result != pdPASS) {
        ESP_LOGW(TAG, "Filled frame queue full, drop current frame");
        frame_return_empty(frame);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/**
 * @brief 向帧添加数据
 * @param frame 目标帧
 * @param data 数据指针
 * @param data_len 数据长度
 * @return ESP_OK 成功，其他值表示失败
 * @note 检查缓冲区是否溢出
 */
esp_err_t frame_add_data(frame_t *frame, const uint8_t *data, size_t data_len)
{
    ESP_RETURN_ON_FALSE(frame && data && data_len, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    if (frame->data_buffer_len < frame->data_len + data_len) {
        ESP_LOGD(TAG, "Frame buffer overflow");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(frame->data + frame->data_len, data, data_len);
    frame->data_len += data_len;
    return ESP_OK;
}

/**
 * @brief 从空缓冲区队列获取一个帧
 * @return 帧指针，NULL表示队列空
 * @note 非阻塞方式获取
 */
frame_t *frame_get_empty(void)
{
    frame_t *this_fb;
    if (xQueueReceive(empty_fb_queue, &this_fb, 0) == pdPASS) {
        return this_fb;
    } else {
        return NULL;
    }
}

/**
 * @brief 从已填充队列获取一个帧
 * @return 帧指针，NULL表示失败
 * @note 阻塞方式获取，直到有数据可用
 */
frame_t *frame_get_filled(void)
{
    frame_t *this_fb;
    if (xQueueReceive(filled_fb_queue, &this_fb, portMAX_DELAY) == pdPASS) {
        return this_fb;
    } else {
        return NULL;
    }
}
