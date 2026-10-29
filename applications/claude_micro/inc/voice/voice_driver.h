/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VOICE_DRIVER_H_
#define _VOICE_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	bool is_voice_driver_working;
	bool is_voice_stop_rx_data;
} T_VOICE_DRIVER_GLOBAL_DATA;

extern T_VOICE_DRIVER_GLOBAL_DATA voice_driver_global_data;

void voice_driver_init(void);
void voice_driver_deinit(void);

#endif /* _VOICE_DRIVER_H_ */
