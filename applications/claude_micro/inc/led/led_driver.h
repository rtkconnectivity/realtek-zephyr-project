/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * LED driver interface for claude_micro (RTL87x2J EVB).
 *
 * Hardware:
 *   RGB LED  (mic key status) : R = P3_5, G = P3_4, B = P3_1
 *   Window 1 / 2 / 3 LEDs     : P2_2 / P2_0 / P3_6 (single colour)
 */

#ifndef _LED_DRIVER_H_
#define _LED_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialise the LED subsystem.
 *
 * Configures the RGB and window LED GPIOs as outputs (off).  Must be called
 * once before any other led_* call.
 *
 * @return 0 on success, negative errno on failure.
 */
int led_driver_init(void);

/**
 * @brief Start mic-active blue blink on the RGB LED (200 ms toggle).
 *
 * Calling this a second time is a no-op.
 */
void led_mic_blink_start(void);

/**
 * @brief Stop mic-active blink and turn the RGB LED off.
 *
 * Safe to call when the blink is not running.
 */
void led_mic_blink_stop(void);

/**
 * @brief Apply per-window LED states received from the host.
 *
 * Each window has its own single-colour LED:
 *   LED_WIN_IDLE    (0) — off
 *   LED_WIN_WORKING (1) — slow blink (1 s period)
 *   LED_WIN_WAITING (2) — fast blink (200 ms period)
 *   LED_WIN_SUCCESS (3) — solid on
 * Values above LED_WIN_SUCCESS are treated as SUCCESS.
 *
 * @param win1  State for window 1 (P2_2).
 * @param win2  State for window 2 (P2_0).
 * @param win3  State for window 3 (P3_6).
 */
void led_set_win_states(uint8_t win1, uint8_t win2, uint8_t win3);

#endif /* _LED_DRIVER_H_ */
