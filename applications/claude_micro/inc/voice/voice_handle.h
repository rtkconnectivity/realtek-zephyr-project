/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VOICE_HANDLE_H_
#define _VOICE_HANDLE_H_

#include <stdbool.h>
#include <stdint.h>
#include <config.h>

typedef struct {
	uint8_t  voice_data_buf[VOICE_BLOCK_SIZE];
	uint32_t voice_data_total_cnt;
} T_VOICE_GLOBAL_DATA;

extern T_VOICE_GLOBAL_DATA voice_global_data;

bool voice_handle_mic_key_pressed(void);
void voice_handle_mic_key_released(void);
void voice_handle_stop_mic(void);

#endif /* _VOICE_HANDLE_H_ */
