/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usb_frame.h
 * @brief USB视频帧缓冲管理头文件
 * @details 定义帧缓冲区的数据结构和操作函数。
 *          用于USB视频数据的接收、缓存和处理。
 * @author Espressif
 * @date 2024
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 帧信息结构
 * @details 包含帧的基本参数信息
 */
typedef struct {
    uint16_t width;      ///< 帧宽度
    uint16_t height;     ///< 帧高度
    uint32_t received;   ///< 已接收的字节数
    uint32_t total;      ///< 帧总字节数
} frame_info_t;

/**
 * @brief 帧缓冲区结构
 * @details 包含数据缓冲区和帧信息
 */
typedef struct {
    size_t data_buffer_len;   ///< 缓冲区最大容量
    size_t data_len;          ///< 当前数据长度
    uint8_t *data;            ///< 数据缓冲区指针
    frame_info_t info;        ///< 帧信息
} frame_t;

/**
 * @brief 分配帧缓冲区
 * @param nb_of_fb 缓冲区数量
 * @param fb_size 每个缓冲区的大小
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t frame_allocate(int nb_of_fb, size_t fb_size);

/**
 * @brief 重置帧状态
 * @param frame 要重置的帧
 */
void frame_reset(frame_t *frame);

/**
 * @brief 将帧返回到空缓冲区队列
 * @param frame 要返回的帧
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t frame_return_empty(frame_t *frame);

/**
 * @brief 将帧发送到已填充队列
 * @param frame 要发送的帧
 * @return ESP_OK 成功，ESP_ERR_NO_MEM 队列已满
 */
esp_err_t frame_send_filled(frame_t *frame);

/**
 * @brief 向帧添加数据
 * @param frame 目标帧
 * @param data 数据指针
 * @param data_len 数据长度
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t frame_add_data(frame_t *frame, const uint8_t *data, size_t data_len);

/**
 * @brief 从空缓冲区队列获取一个帧
 * @return 帧指针，NULL表示队列空
 */
frame_t *frame_get_empty(void);

/**
 * @brief 从已填充队列获取一个帧
 * @return 帧指针，NULL表示失败
 */
frame_t *frame_get_filled(void);

#ifdef __cplusplus
}
#endif
