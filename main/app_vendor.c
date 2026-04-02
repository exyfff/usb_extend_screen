/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_vendor.c
 * @brief USB Vendor类实现（自定义视频传输）
 * @details 实现通过USB Vendor类传输视频数据的功能。
 *          支持JPEG、RGB565、RGB888等格式，包含帧缓冲管理、
 *          丢帧统计、性能优化等功能。
 * @author Espressif
 * @date 2024
 */

#include <inttypes.h>
#include "esp_log.h"
#include "app_usb.h"
#include "app_lcd.h"
#include "esp_timer.h"
#include "usb_frame.h"

static const char *TAG = "app_vendor";
/* 当前正在处理的帧 */
static frame_t *current_frame = NULL;
/* 丢帧计数器 */
static uint32_t dropped_frames = 0;

//--------------------------------------------------------------------+
/* Vendor回调函数 */
//--------------------------------------------------------------------+

/* USB Vendor接收缓冲区大小 */
#define CONFIG_USB_VENDOR_RX_BUFSIZE 512

/* -- 显示数据包类型定义 */
#define UDISP_TYPE_RGB565  0  /* RGB565格式 */
#define UDISP_TYPE_RGB888  1  /* RGB888格式 */
#define UDISP_TYPE_YUV420  2  /* YUV420格式 */
#define UDISP_TYPE_JPG     3  /* JPEG格式 */
/**
 * @brief USB显示帧头结构
 * @details 包含帧的基本信息：类型、尺寸、位置、数据大小等
 */
typedef struct {
    uint16_t crc16;              /* CRC16校验 */
    uint8_t  type;               /* 数据类型 */
    uint8_t  cmd;                /* 命令字 */
    uint16_t x;                  /* X坐标 */
    uint16_t y;                  /* Y坐标 */
    uint16_t width;              /* 宽度 */
    uint16_t height;             /* 高度 */
    uint32_t frame_id: 10;       /* 帧ID（0-1023） */
    uint32_t payload_total: 22;  /* 数据总大小（32位对齐） */
} __attribute__((packed)) udisp_frame_header_t;

/**
 * @brief 视频传输任务
 * @param pvParameter 任务参数（未使用）
 * @details 从帧缓冲队列获取数据并显示到LCD
 * @note 运行在核心0，与USB任务（核心1）分离
 */
void transfer_task(void *pvParameter)
{
    /* 性能优化：分配10个缓冲区减少丢帧 */
    frame_allocate(10, JPEG_BUFFER_SIZE);
    frame_t *usr_frame = NULL;
    /* 主循环：阻塞等待帧数据，无需额外delay */
    while (1) {
        usr_frame = frame_get_filled();  /* 阻塞等待，无需额外delay */
        app_lcd_draw(usr_frame->data, usr_frame->info.total, usr_frame->info.width, usr_frame->info.height);
        frame_return_empty(usr_frame);
        /* 性能优化：frame_get_filled已阻塞等待，任务优先级已调整无需主动让出 */
    }
}

/**
 * @brief 跳过当前帧的剩余数据
 * @param frame_info 帧信息结构
 * @param len 要跳过的数据长度
 * @return true 帧已接收完毕，false 还有数据待接收
 * @note 用于处理缓冲区不足时的丢帧场景
 */
static bool buffer_skip(frame_info_t *frame_info, uint32_t len)
{
    if (frame_info->received + len >= frame_info->total) {
        return true;
    }
    frame_info->received += len;
    return false;
}

/**
 * @brief 记录丢帧事件
 * @param reason 丢帧原因
 * @details 每50次丢帧输出一次警告，避免日志刷屏
 */
static void log_drop_event(const char *reason)
{
    dropped_frames++;
    if ((dropped_frames % 50) == 0) {
        ESP_LOGW(TAG, "Dropped frames: %"PRIu32" (latest reason: %s)", dropped_frames, reason);
    } else {
        ESP_LOGV(TAG, "Drop frame: %s", reason);
    }
}

/**
 * @brief 向帧缓冲区添加数据
 * @param frame 目标帧
 * @param buf 数据缓冲区
 * @param len 数据长度
 * @return true 帧已接收完整，false 还需要更多数据
 * @note 如果缓冲区溢出会自动丢帧
 */
static bool buffer_fill(frame_t *frame, uint8_t *buf, uint32_t len)
{
    if (len == 0) {
        return false;
    }

    if (frame_add_data(frame, buf, len) != ESP_OK) {
        log_drop_event("overflow");
        frame_return_empty(frame);
        return true;
    }
    frame->info.received += len;

    if (frame->info.received >= frame->info.total) {
        if (frame_send_filled(frame) != ESP_OK) {
            log_drop_event("filled queue full");
        }
        return true;
    }
    return false;
}

/**
 * @brief USB Vendor接收回调函数
 * @param itf 接口号
 * @details 处理从主机接收到的视频数据
 *          支持多种格式，自动处理帧边界和缓冲区管理
 */
void tud_vendor_rx_cb(uint8_t itf)
{
    /* 静态变量：接收状态管理 */
    static uint8_t rx_buf[CONFIG_USB_VENDOR_RX_BUFSIZE];
    static bool skip_frame = false;
    static frame_info_t skip_frame_info = {0};

    while (tud_vendor_n_available(itf)) {
        int read_res = tud_vendor_n_read(itf, rx_buf, CONFIG_USB_VENDOR_RX_BUFSIZE);
        if (read_res > 0) {
            if (!current_frame && !skip_frame) {
                udisp_frame_header_t *pblt = (udisp_frame_header_t *)rx_buf;
                switch (pblt->type) {
                case UDISP_TYPE_RGB565:
                case UDISP_TYPE_RGB888:
                case UDISP_TYPE_YUV420:
                case UDISP_TYPE_JPG: {
                    if (pblt->x != 0 || pblt->y != 0 || pblt->width != 1024 || pblt->height != 600) {
                        break;
                    }

    /* 性能优化：FPS统计每200帧输出一次，减少串口负载 */
#if CONFIG_EXAMPLE_ENABLE_PRINT_FPS_RATE_VALUE
                    static int fps_count = 0;
                    static int64_t start_time = 0;
                    fps_count++;
                    if (fps_count == 200) {
                        int64_t end_time = esp_timer_get_time();
                        ESP_LOGI(TAG, "Input fps: %.1f", 1000000.0 / ((end_time - start_time) / 200.0));
                        start_time = end_time;
                        fps_count = 0;
                    }
#endif

                    current_frame = frame_get_empty();
                    if (current_frame) {
                        current_frame->info.width = pblt->width;
                        current_frame->info.height = pblt->height;
                        current_frame->info.total = pblt->payload_total;
                        current_frame->info.received = 0;
                        /* 调试信息：打印帧参数和预期大小 */
                        if (buffer_fill(current_frame, &rx_buf[sizeof(udisp_frame_header_t)], read_res - sizeof(udisp_frame_header_t))) {
                            current_frame = NULL;
                        }
                    } else {
                        memset(&skip_frame_info, 0, sizeof(skip_frame_info));
                        skip_frame_info.total = pblt->payload_total;
                        skip_frame = true;
                        buffer_skip(&skip_frame_info, read_res - sizeof(udisp_frame_header_t));
                        ESP_LOGE(TAG, "Get frame is null - dropped_frames: %"PRIu32", frame_size: %"PRIu32, dropped_frames, pblt->payload_total);
                        log_drop_event("no empty frame");
                    }
                    break;
                }
                default:
                    ESP_LOGE(TAG, "error cmd");
                    break;
                }
            } else if (skip_frame) {
                if (buffer_skip(&skip_frame_info, read_res)) {
                    current_frame = NULL;
                    skip_frame = false;
                }
            } else {
                if (buffer_fill(current_frame, rx_buf, read_res)) {
                    current_frame = NULL;
                }
            }
        }
    }
}

/**
 * @brief 初始化Vendor类
 * @return ESP_OK 成功
 * @details 创建视频传输任务
 */
esp_err_t app_vendor_init(void)
{
    xTaskCreatePinnedToCore(transfer_task, "transfer_task", 4096, NULL, CONFIG_VENDOR_TASK_PRIORITY, NULL, 0);
    return ESP_OK;
}
