/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Button event handler for claude_micro.
 *
 * btn1 (GPIOA_13 / P2_1): voice start/stop only — no HID side-effect.
 * btn2 (GPIOA_17 / P2_5): window 1 YES key (HID bit 0).
 * btn3 (GPIOA_4  / P0_4): window 2 YES key (HID bit 1).
 * btn4 (GPIOA_10 / P1_6): window 3 YES key (HID bit 2).
 *
 * IMPORTANT: GPIO callbacks run in ISR context.
 * - Mic operations (voice_handle_*) must run in thread context because they
 *   call k_sem_take() to wait for the I2S thread to stop cleanly before
 *   reinitialising the codec.  They are therefore deferred to the system
 *   workqueue via k_work.
 * - HID send only latches the button mask; usb_hid.c submits the report
 *   from a work item, so it is safe to call from ISR context.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/* Implemented in src/voice/voice_handle.c */
bool voice_handle_mic_key_pressed(void);
void voice_handle_mic_key_released(void);

/* Implemented in src/usb/usb_hid.c */
void app_usb_hid_send_win_key(uint8_t win_mask, bool pressed);

/* Implemented in src/led/led_driver.c */
void led_mic_blink_start(void);
void led_mic_blink_stop(void);

/* ── Deferred work items for mic (cannot block in ISR) ───────────────────── */
static void mic_press_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	led_mic_blink_start();
	voice_handle_mic_key_pressed();
}

static void mic_release_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	voice_handle_mic_key_released();
	led_mic_blink_stop();
}

static K_WORK_DEFINE(mic_press_work,   mic_press_fn);
static K_WORK_DEFINE(mic_release_work, mic_release_fn);

/* ── btn1: voice start / stop ────────────────────────────────────────────── */
void key_handle_mic_press(void)
{
	LOG_INF("btn1 press → mic start");
	k_work_submit(&mic_press_work);
}

void key_handle_mic_release(void)
{
	LOG_INF("btn1 release → mic stop");
	k_work_submit(&mic_release_work);
}

/* ── btn2: window 1 YES ──────────────────────────────────────────────────── */
void key_handle_hid_press(void)
{
	LOG_INF("btn2 press → win1 YES down");
	app_usb_hid_send_win_key(0x01u, true);
}

void key_handle_hid_release(void)
{
	LOG_INF("btn2 release → win1 YES up");
	app_usb_hid_send_win_key(0x01u, false);
}

/* ── btn3: window 2 YES ──────────────────────────────────────────────────── */
void key_handle_win2_press(void)
{
	LOG_INF("btn3 press → win2 YES down");
	app_usb_hid_send_win_key(0x02u, true);
}

void key_handle_win2_release(void)
{
	LOG_INF("btn3 release → win2 YES up");
	app_usb_hid_send_win_key(0x02u, false);
}

/* ── btn4: window 3 YES ──────────────────────────────────────────────────── */
void key_handle_win3_press(void)
{
	LOG_INF("btn4 press → win3 YES down");
	app_usb_hid_send_win_key(0x04u, true);
}

void key_handle_win3_release(void)
{
	LOG_INF("btn4 release → win3 YES up");
	app_usb_hid_send_win_key(0x04u, false);
}
