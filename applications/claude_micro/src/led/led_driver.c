/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * LED driver for claude_micro (RTL87x2J EVB).
 *
 * Hardware mapping (see board overlay, rgb_leds / win_leds nodes):
 *   RGB LED (mic key)  R : P3_5 / GPIOA_25
 *                      G : P3_4 / GPIOA_24
 *                      B : P3_1 / GPIOA_21
 *   Window 1 LED       P2_2 / GPIOA_14
 *   Window 2 LED       P2_0 / GPIOA_12
 *   Window 3 LED       P3_6 / GPIOA_26
 *
 * Effects:
 *   RGB      : blue blink (200 ms toggle) while recording, off otherwise.
 *   Window n : IDLE    — off
 *              WORKING — slow blink (1 s period)
 *              WAITING — fast blink (200 ms period)
 *              SUCCESS — solid on
 *
 * All blinking is driven by one delayable work item ticking every
 * LED_TICK_MS on the system workqueue; it only runs while some LED blinks.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "led/led_driver.h"
#include "config.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/* ── Timing (in ticks of LED_TICK_MS) ────────────────────────────────────── */
#define LED_TICK_MS        100U
#define MIC_BLINK_TICKS    2U    /* 200 ms on / 200 ms off */
#define WIN_SLOW_TICKS     5U    /* 500 ms on / 500 ms off */
#define WIN_FAST_TICKS     1U    /* 100 ms on / 100 ms off */

#define WIN_NUM            3U

/* ── Device-tree handles ──────────────────────────────────────────────────── */
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_r), gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_g), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_b), gpios);

static const struct gpio_dt_spec win_leds[WIN_NUM] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(win1_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(win2_led), gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(win3_led), gpios),
};

/* ── State ────────────────────────────────────────────────────────────────── */
static bool mic_active;
static uint8_t win_state[WIN_NUM];   /* LED_WIN_* */
static uint32_t tick;
K_MUTEX_DEFINE(led_mutex);

static void led_tick_fn(struct k_work *w);
static K_WORK_DELAYABLE_DEFINE(led_tick_work, led_tick_fn);

static bool blink_on(uint32_t half_period_ticks)
{
	return ((tick / half_period_ticks) % 2U) == 0U;
}

static bool win_led_on(uint8_t state)
{
	switch (state) {
	case LED_WIN_WORKING:
		return blink_on(WIN_SLOW_TICKS);
	case LED_WIN_WAITING:
		return blink_on(WIN_FAST_TICKS);
	case LED_WIN_SUCCESS:
		return true;
	default:
		return false;
	}
}

static bool need_tick(void)
{
	if (mic_active) {
		return true;
	}
	for (uint8_t i = 0U; i < WIN_NUM; i++) {
		if (win_state[i] == LED_WIN_WORKING ||
		    win_state[i] == LED_WIN_WAITING) {
			return true;
		}
	}
	return false;
}

/* Push current state to the GPIOs and keep the tick running if needed.
 * Caller must hold led_mutex.
 */
static void led_update(void)
{
	bool mic_on = mic_active && blink_on(MIC_BLINK_TICKS);

	(void)gpio_pin_set_dt(&led_r, 0);
	(void)gpio_pin_set_dt(&led_g, 0);
	(void)gpio_pin_set_dt(&led_b, mic_on ? 1 : 0);

	for (uint8_t i = 0U; i < WIN_NUM; i++) {
		(void)gpio_pin_set_dt(&win_leds[i], win_led_on(win_state[i]) ? 1 : 0);
	}

	if (need_tick()) {
		/* No-op if already scheduled */
		k_work_schedule(&led_tick_work, K_MSEC(LED_TICK_MS));
	}
}

static void led_tick_fn(struct k_work *w)
{
	ARG_UNUSED(w);

	k_mutex_lock(&led_mutex, K_FOREVER);
	tick++;
	led_update();
	k_mutex_unlock(&led_mutex);
}

static int led_config_output(const struct gpio_dt_spec *spec)
{
	int ret;

	if (!gpio_is_ready_dt(spec)) {
		LOG_ERR("LED GPIO %s not ready", spec->port->name);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("LED %s pin %u configure failed: %d",
			spec->port->name, spec->pin, ret);
	}
	return ret;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int led_driver_init(void)
{
	const struct gpio_dt_spec *rgb[] = {&led_r, &led_g, &led_b};
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(rgb); i++) {
		ret = led_config_output(rgb[i]);
		if (ret) {
			return ret;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(win_leds); i++) {
		ret = led_config_output(&win_leds[i]);
		if (ret) {
			return ret;
		}
	}

	LOG_INF("LED driver init done (RGB mic + %u window LEDs)", WIN_NUM);
	return 0;
}

void led_mic_blink_start(void)
{
	k_mutex_lock(&led_mutex, K_FOREVER);
	if (!mic_active) {
		mic_active = true;
		tick = 0U;   /* start with LED on */
		led_update();
		LOG_INF("LED mic blink started");
	}
	k_mutex_unlock(&led_mutex);
}

void led_mic_blink_stop(void)
{
	k_mutex_lock(&led_mutex, K_FOREVER);
	if (mic_active) {
		mic_active = false;
		led_update();
		LOG_INF("LED mic blink stopped");
	}
	k_mutex_unlock(&led_mutex);
}

void led_set_win_states(uint8_t win1, uint8_t win2, uint8_t win3)
{
	const uint8_t states[WIN_NUM] = {win1, win2, win3};

	k_mutex_lock(&led_mutex, K_FOREVER);
	for (uint8_t i = 0U; i < WIN_NUM; i++) {
		win_state[i] = MIN(states[i], (uint8_t)LED_WIN_SUCCESS);
	}
	led_update();
	k_mutex_unlock(&led_mutex);

	LOG_INF("LED win states: %u %u %u", win1, win2, win3);
}
