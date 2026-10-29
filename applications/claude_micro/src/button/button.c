/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Four-button GPIO driver for claude_micro (RTL87x2J EVB).
 *
 * test-pin1 (GPIOA_13 / P2_1, KEY4): start / stop voice recording.
 * test-pin2 (GPIOA_17 / P2_5, KEY5): send / release HID key (window 1).
 * test-pin3 (GPIOA_4  / P0_4)      : window 2 YES key.
 * test-pin4 (GPIOA_10 / P1_6)      : window 3 YES key.
 *
 * Buttons are active-low with internal pull-up. Interrupts use
 * GPIO_INT_EDGE_BOTH (gpio_bee emulates it by flipping the level polarity
 * in the ISR); the pin level is read in the callback to decide
 * press/release, and repeated events of the same state are dropped.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/* ── Device tree ──────────────────────────────────────────────────────────── */
#define TEST_PIN_NODE DT_NODELABEL(test_pin)

/* ── Event callbacks (implemented in button_handle.c) ────────────────────── */
void key_handle_mic_press(void);
void key_handle_mic_release(void);
void key_handle_hid_press(void);
void key_handle_hid_release(void);
void key_handle_win2_press(void);
void key_handle_win2_release(void);
void key_handle_win3_press(void);
void key_handle_win3_release(void);

struct button {
	const char *name;
	const char *func;
	struct gpio_dt_spec spec;
	void (*on_press)(void);
	void (*on_release)(void);
	struct gpio_callback cb_data;
	bool pressed;
};

static struct button buttons[] = {
	{
		.name = "BTN1", .func = "voice start/stop",
		.spec = GPIO_DT_SPEC_GET(TEST_PIN_NODE, test_pin1),
		.on_press = key_handle_mic_press,
		.on_release = key_handle_mic_release,
	},
	{
		.name = "BTN2", .func = "HID key",
		.spec = GPIO_DT_SPEC_GET(TEST_PIN_NODE, test_pin2),
		.on_press = key_handle_hid_press,
		.on_release = key_handle_hid_release,
	},
	{
		.name = "BTN3", .func = "win2 YES",
		.spec = GPIO_DT_SPEC_GET(TEST_PIN_NODE, test_pin3),
		.on_press = key_handle_win2_press,
		.on_release = key_handle_win2_release,
	},
	{
		.name = "BTN4", .func = "win3 YES",
		.spec = GPIO_DT_SPEC_GET(TEST_PIN_NODE, test_pin4),
		.on_press = key_handle_win3_press,
		.on_release = key_handle_win3_release,
	},
};

/* ── Shared ISR ───────────────────────────────────────────────────────────── */
static void button_isr(const struct device *port, struct gpio_callback *cb,
		       gpio_port_pins_t pins)
{
	struct button *btn = CONTAINER_OF(cb, struct button, cb_data);
	int level;
	bool pressed;

	ARG_UNUSED(pins);

	level = gpio_pin_get_dt(&btn->spec);
	if (level < 0) {
		return;
	}

	/* Active-low button: pin LOW = pressed */
	pressed = (level == 0);
	if (pressed == btn->pressed) {
		return;
	}
	btn->pressed = pressed;

	LOG_INF("[%s] %s_%u %s (%s)", btn->name, port->name, btn->spec.pin,
		pressed ? "pressed " : "released", btn->func);

	if (pressed) {
		btn->on_press();
	} else {
		btn->on_release();
	}
}

/* ── Helper ───────────────────────────────────────────────────────────────── */
static int setup_button(struct button *btn)
{
	const struct gpio_dt_spec *spec = &btn->spec;
	int ret;

	if (!gpio_is_ready_dt(spec)) {
		LOG_ERR("GPIO %s pin %u not ready", spec->port->name, spec->pin);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_INPUT | GPIO_PULL_UP);
	if (ret) {
		LOG_ERR("configure pin %u failed: %d", spec->pin, ret);
		return ret;
	}

	btn->pressed = false;
	gpio_init_callback(&btn->cb_data, button_isr, BIT(spec->pin));

	ret = gpio_add_callback_dt(spec, &btn->cb_data);
	if (ret) {
		LOG_ERR("add callback pin %u failed: %d", spec->pin, ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(spec, GPIO_INT_EDGE_BOTH);
	if (ret) {
		LOG_ERR("interrupt configure pin %u failed: %d", spec->pin, ret);
		return ret;
	}

	return 0;
}

/* ── Public init ──────────────────────────────────────────────────────────── */
void button_init(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		struct button *btn = &buttons[i];

		if (setup_button(btn) == 0) {
			LOG_INF("%s init done (%s_%u, %s)", btn->name,
				btn->spec.port->name, btn->spec.pin, btn->func);
		}
	}
}
