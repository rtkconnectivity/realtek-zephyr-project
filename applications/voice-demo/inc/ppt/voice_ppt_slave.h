/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	VOICE_PPT_SLAVE_STATE_IDLE,
	VOICE_PPT_SLAVE_STATE_PAIRING,
	VOICE_PPT_SLAVE_STATE_CONNECTED,
	VOICE_PPT_SLAVE_STATE_DISCONNECTED,
} T_VOICE_PPT_SLAVE_STATE;

typedef struct {
	T_VOICE_PPT_SLAVE_STATE state;
	bool is_bonded;
} T_VOICE_PPT_SLAVE_DATA;

extern T_VOICE_PPT_SLAVE_DATA voice_ppt_slave_data;

/**
 * @brief Initialize 2.4G slave role and mSBC decoder (call once at startup)
 */
void voice_ppt_slave_init(void);

/**
 * @brief Enable 2.4G slave (call after init; starts pairing or reconnect)
 */
void voice_ppt_slave_enable(void);

/**
 * @brief Initialize USB audio device and register mic ops (call once before PPT init)
 */
void app_usb_audio_init(void);

/**
 * @brief Send decoded PCM audio over USB.
 *
 * Implemented in src/ppt/usb_audio.c.
 * Called by voice_ppt_slave.c after each mSBC frame is decoded.
 *
 * @param pcm_data  Pointer to raw 16-bit mono PCM samples
 * @param pcm_size  Number of bytes (240 bytes per mSBC frame at 16kHz)
 */
void app_usb_audio_send(const uint8_t *pcm_data, int pcm_size);
