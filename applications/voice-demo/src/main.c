/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <kscan.h>
#include <ble/ble.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

int main()
{
	LOG_DBG("");
#if IS_ENABLED(CONFIG_KSCAN)
	if (kscan_init(DEVICE_DT_GET(DT_CHOSEN(kscan))) != 0) {
		return -ENOTSUP;
	}
#endif
#if IS_ENABLED(CONFIG_BT)
	ble_init();
#endif
}