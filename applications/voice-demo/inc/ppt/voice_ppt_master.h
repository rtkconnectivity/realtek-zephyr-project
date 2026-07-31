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
 * @brief Send one encoded frame pair over 2.4G as two PPT packets.
 *
 * SW_MSBC_ENC: 120-byte input → two 62-byte packets
 *   bytes[0..59]   = first mSBC frame, bytes[60..119] = second mSBC frame
 * SW_SBC_ENC:  72-byte input → two 38-byte packets
 *   bytes[0..35]   = first SBC frame,  bytes[36..71]  = second SBC frame
 *
 * @param frame_pair  Pointer to encoded frame pair buffer (size = VOICE_REPORT_FRAME_SIZE)
 */
void app_ppt_send_voice_data(const uint8_t *frame_pair);

/**
 * @brief Reconnect to bonded slave if currently disconnected (call on key press).
 *        No-op if not bonded or already connected/pairing.
 */
void voice_ppt_master_try_reconnect(void);