/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define PPT_PAIR_RSSI -65
#define PPT_PAIR_TIME_MAX_COUNT 30

typedef enum {
	VOICE_PPT_MASTER_STATE_IDLE,
	VOICE_PPT_MASTER_STATE_PAIRING,
	VOICE_PPT_MASTER_STATE_CONNECTED,
	VOICE_PPT_MASTER_STATE_DISCONNECTED,
} T_VOICE_PPT_MASTER_STATE;

typedef struct {
	T_VOICE_PPT_MASTER_STATE state;
	bool is_bonded;
} T_VOICE_PPT_MASTER_DATA;

extern T_VOICE_PPT_MASTER_DATA voice_ppt_master_data;

/**
 * @brief Initialize 2.4G master role (call once at startup)
 */
void voice_ppt_master_init(void);

/**
 * @brief Enable 2.4G master (call after init; starts pairing or reconnect)
 */
void voice_ppt_master_enable(void);

/**
 * @brief Send one mSBC frame pair (120 bytes) over 2.4G as two 62-byte packets.
 *
 * @param msbc_frame_pair  Pointer to 120-byte buffer:
 *                         bytes[0..59]   = first mSBC frame
 *                         bytes[60..119] = second mSBC frame
 */
void app_ppt_send_voice_data(const uint8_t *msbc_frame_pair);
