/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * claude_micro — USB audio dongle main entry.
 *
 * Initialisation order:
 *   1. USB (UAC2 mic + HID) — class ops are registered first, then the
 *      device_next stack is initialised and enabled.
 *   2. Button GPIO interrupts.
 *
 * The voice driver (I2S / codec) is started on-demand when the button is
 * pressed; see src/button/button_handle.c.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led/led_driver.h"

LOG_MODULE_REGISTER(app, CONFIG_APP_LOG_LEVEL);

/* Forward declarations — implemented in their respective modules. */
void app_usb_audio_init(void);
void app_usb_hid_init(void);
int app_usbd_init(void);
void button_init(void);

int main(void)
{
	LOG_INF("claude_micro starting");

	app_usb_audio_init();
	app_usb_hid_init();
	if (app_usbd_init() != 0) {
		LOG_ERR("USB init failed");
	}
	button_init();

	if (led_driver_init() != 0) {
		LOG_ERR("LED init failed");
	}

	LOG_INF("claude_micro init done");
	return 0;
}
