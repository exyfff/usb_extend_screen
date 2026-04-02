/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "uac_descriptors.h"

//------------- CLASS -------------//
// The number of audio interfaces
#define CFG_TUD_AUDIO             1

//--------------------------------------------------------------------
// AUDIO CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// Enable feedback EP
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                             1

// Enable/disable conversion from 16.16 to 10.14 format on full-speed devices
#if CONFIG_UAC_SUPPORT_MACOS
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION              1
#endif

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                TUD_AUDIO_DEVICE_DESC_LEN

// How many formats are used, need to adjust USB descriptor if changed
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS                               1

#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                         DEFAULT_SAMPLE_RATE
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                           SPEAK_CHANNEL_NUM
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                           MIC_CHANNEL_NUM

// 16bit in 16bit slots
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX                  16
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX          2
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX                  16

// MIC EP IN - 仅在启用麦克风时有效
#if MIC_CHANNEL_NUM
#define CFG_TUD_AUDIO_ENABLE_EP_IN                1
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN    ((CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE / 1000 * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX) + 8)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ      CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN * (MIC_INTERVAL_MS + 1)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX         CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_IN
#else
#define CFG_TUD_AUDIO_ENABLE_EP_IN                0
#endif

// SPK EP OUT - 仅在启用扬声器时有效
#if SPEAK_CHANNEL_NUM
#define CFG_TUD_AUDIO_ENABLE_EP_OUT               1
#else
#define CFG_TUD_AUDIO_ENABLE_EP_OUT               0
#endif

// SPK +4 for audio feedback
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT   ((CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE / 1000 * CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX * CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX) + 8)

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT * (SPK_INTERVAL_MS + 1)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        CFG_TUD_AUDIO_FUNC_1_FORMAT_1_EP_SZ_OUT // Maximum EP IN size for all AS alternate settings used

// Number of AS interfaces: speaker + mic need 2, single direction uses 1
#if SPEAK_CHANNEL_NUM && MIC_CHANNEL_NUM
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT             2
#else
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT             1
#endif

// Size of control request buffer
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ    64

#ifdef __cplusplus
}
#endif
