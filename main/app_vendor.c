/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "esp_log.h"
#include "app_usb.h"
#include "app_lcd.h"
#include "esp_timer.h"
#include "usb_frame.h"

static const char *TAG = "app_vendor";
static frame_t *current_frame = NULL;
static uint32_t dropped_frames = 0;

//--------------------------------------------------------------------+
// Vendor callbacks
//--------------------------------------------------------------------+

#define CONFIG_USB_VENDOR_RX_BUFSIZE 512

// -- Display Packets
#define UDISP_TYPE_RGB565  0
#define UDISP_TYPE_RGB888  1
#define UDISP_TYPE_YUV420  2
#define UDISP_TYPE_JPG     3

typedef struct {
    uint16_t crc16;
    uint8_t  type;
    uint8_t cmd;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t frame_id: 10;
    uint32_t payload_total: 22; //padding 32bit align
} __attribute__((packed)) udisp_frame_header_t;

void transfer_task(void *pvParameter)
{
    frame_allocate(10, JPEG_BUFFER_SIZE);  // 10个缓冲区减少丢帧
    frame_t *usr_frame = NULL;
    while (1) {
        usr_frame = frame_get_filled();  // 阻塞等待，无需额外delay
        app_lcd_draw(usr_frame->data, usr_frame->info.total, usr_frame->info.width, usr_frame->info.height);
        frame_return_empty(usr_frame);
        // 移除vTaskDelay：frame_get_filled已阻塞等待，任务优先级已调整无需主动让出
    }
}

static bool buffer_skip(frame_info_t *frame_info, uint32_t len)
{
    if (frame_info->received + len >= frame_info->total) {
        return true;
    }
    frame_info->received += len;
    return false;
}

static void log_drop_event(const char *reason)
{
    dropped_frames++;
    if ((dropped_frames % 50) == 0) {
        ESP_LOGW(TAG, "Dropped frames: %"PRIu32" (latest reason: %s)", dropped_frames, reason);
    } else {
        ESP_LOGV(TAG, "Drop frame: %s", reason);
    }
}

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

void tud_vendor_rx_cb(uint8_t itf)
{
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

                    // FPS统计：每200帧输出一次，减少串口负载
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
                        ESP_LOGD(TAG, "read_res: %d, rx bblt x:%d y:%d w:%d h:%d total:%"PRIu32" (%d)", read_res, pblt->x, pblt->y, pblt->width, pblt->height, current_frame->info.total, (pblt->width) * (pblt->height) * 2);
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

esp_err_t app_vendor_init(void)
{
    xTaskCreatePinnedToCore(transfer_task, "transfer_task", 4096, NULL, CONFIG_VENDOR_TASK_PRIORITY, NULL, 0);
    return ESP_OK;
}
