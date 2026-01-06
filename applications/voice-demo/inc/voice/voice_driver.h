/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include "config.h"
#include <zephyr/audio/codec.h>

/*============================================================================*
 *                              Macros
 *============================================================================*/
/*voice config*/
#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
#define VOICE_PCM_FRAME_SIZE 240
#define VOICE_PCM_FRAME_CNT 2
#define VOICE_GDMA_FRAME_SIZE (VOICE_PCM_FRAME_SIZE * VOICE_PCM_FRAME_CNT)
#define VOICE_FRAME_SIZE_AFTER_ENC 57
#define VOICE_REPORT_FRAME_SIZE ((VOICE_FRAME_SIZE_AFTER_ENC + 3) * VOICE_PCM_FRAME_CNT)  /* 3 bytes header per frame */

#elif (VOICE_ENC_TYPE == SW_SBC_ENC)
#define VOICE_PCM_FRAME_CNT              2
#define VOICE_PCM_FRAME_SIZE             256
#define VOICE_FRAME_SIZE_AFTER_ENC       (2 * BIT_POOL_SIZE + 8)
#define VOICE_REPORT_FRAME_SIZE          (VOICE_FRAME_SIZE_AFTER_ENC * VOICE_PCM_FRAME_CNT)
#define VOICE_GDMA_FRAME_SIZE            (VOICE_PCM_FRAME_SIZE * VOICE_PCM_FRAME_CNT)
#define BIT_POOL_SIZE                    14

#elif (VOICE_ENC_TYPE == SW_IMA_ADPCM_ENC)
#define VOICE_PCM_FRAME_CNT              1
#define VOICE_PCM_FRAME_SIZE             512
#define VOICE_FRAME_SIZE_AFTER_ENC       128
#define VOICE_FRAME_HEADER_SIZE          6
#define VOICE_REPORT_FRAME_SIZE          (VOICE_FRAME_SIZE_AFTER_ENC * VOICE_PCM_FRAME_CNT + VOICE_FRAME_HEADER_SIZE)
#define VOICE_GDMA_FRAME_SIZE            (VOICE_PCM_FRAME_SIZE * VOICE_PCM_FRAME_CNT)

#endif

/*============================================================================*
 *                         Types
 *============================================================================*/
typedef struct
{
    bool is_allowed_to_enter_dlps;  /* to indicate whether to allow to enter dlps or not */
    bool is_voice_driver_working;  /* to indicate whether voice driver is working or not */
    bool is_voice_stop_rx_data;    /* to indicate whether voice driver is stopped or not */
} T_VOICE_DRIVER_GLOBAL_DATA;

/*============================================================================*
*                        Export Global Variables
*============================================================================*/
extern T_VOICE_DRIVER_GLOBAL_DATA voice_driver_global_data;

extern const struct device *dev_uart;
extern const struct device *dev_i2s;
extern const struct device *dev_codec;

/*============================================================================*
 *                         Functions
 *============================================================================*/

void voice_driver_init(void);
void voice_driver_deinit(void);
void i2s_codec_rx_sample(void);