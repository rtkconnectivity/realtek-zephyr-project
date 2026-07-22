/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include "button.h"
#include "button_handle.h"
#include <ble/ble.h>
#include <zephyr/logging/log.h>
#if IS_ENABLED(CONFIG_VOICE_PPT_MASTER)
#include <ppt/voice_ppt_master.h>
#elif IS_ENABLED(CONFIG_VOICE_PPT_SLAVE)
#include <ppt/voice_ppt_slave.h>
#endif

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

int main()
{
	LOG_DBG("");
// #if IS_ENABLED(CONFIG_KSCAN)
// 	if (kscan_init(DEVICE_DT_GET(DT_CHOSEN(kscan))) != 0) {
// 		return -ENOTSUP;
// 	}
// #endif
#if IS_ENABLED(CONFIG_BT)
	ble_init();
#endif
#if IS_ENABLED(CONFIG_VOICE_PPT_MASTER)
	voice_ppt_master_init();
	voice_ppt_master_enable();
#elif IS_ENABLED(CONFIG_VOICE_PPT_SLAVE)
	app_usb_audio_init();
	voice_ppt_slave_init();
	voice_ppt_slave_enable();
#endif
	voice_test_button_init();
}