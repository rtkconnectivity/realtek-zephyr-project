/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB HID for claude_micro (USB device_next).
 *
 * IN  report 0x01 (2 bytes): [id, win buttons bit0-2]
 * OUT report 0x02 (4 bytes): [id, win1, win2, win3] LED state 0-3
 *
 * Input reports are submitted from a work item, so app_usb_hid_send_win_key()
 * is safe to call from ISR context.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/drivers/usb/usb_buf.h>
#include <zephyr/usb/class/usbd_hid.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/logging/log.h>
#include <config.h>
#include "led/led_driver.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/* ── HID report descriptor ────────────────────────────────────────────────── */
static const uint8_t hid_report_desc[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(0x0C),                        /* Consumer Control */
	HID_COLLECTION(HID_COLLECTION_APPLICATION),

	/* IN report 0x01: 3 window-YES buttons (Button page, usages 1-3) */
	HID_REPORT_ID(HID_REPORT_ID_BUTTONS),
	HID_USAGE_PAGE(0x09),               /* Button */
	HID_USAGE_MIN8(1),
	HID_USAGE_MAX8(3),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(1),
	HID_REPORT_SIZE(1),
	HID_REPORT_COUNT(3),
	HID_INPUT(0x02),                    /* Data, Variable, Absolute */
	/* 5-bit padding */
	HID_REPORT_SIZE(1),
	HID_REPORT_COUNT(5),
	HID_INPUT(0x03),                    /* Constant */

	/* OUT report 0x02: per-window LED state from host (3 bytes, 0-3) */
	HID_REPORT_ID(HID_REPORT_ID_LED),
	HID_USAGE_PAGE(0xFF),               /* Vendor defined */
	HID_USAGE_MIN8(1),
	HID_USAGE_MAX8(3),
	HID_LOGICAL_MIN8(0),
	HID_LOGICAL_MAX8(3),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(3),
	HID_OUTPUT(0x02),                   /* Data, Variable, Absolute */

	HID_END_COLLECTION,
};

#define HID_IN_REPORT_SIZE  2u

/* ── HID device handle ────────────────────────────────────────────────────── */
static const struct device *hid_dev = DEVICE_DT_GET(DT_NODELABEL(hid_dev_0));
static atomic_t hid_ready;

/* Input report buffer must be UDC aligned */
UDC_STATIC_BUF_DEFINE(hid_in_report, HID_IN_REPORT_SIZE);
static atomic_t hid_in_busy;
static atomic_t hid_win_mask;   /* pending button state, sent by work item */

/* ── LED OUT report deferred work ─────────────────────────────────────────── */
static uint8_t led_out_buf[4];   /* [report_id, win1, win2, win3] */

static void led_apply_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	LOG_INF("LED apply: win1=%u win2=%u win3=%u",
		led_out_buf[1], led_out_buf[2], led_out_buf[3]);
	led_set_win_states(led_out_buf[1], led_out_buf[2], led_out_buf[3]);
}

static K_WORK_DEFINE(led_apply_work, led_apply_fn);

/* buf: [report_id, win1, win2, win3] or [win1, win2, win3] */
static int hid_handle_led_report(const uint16_t len, const uint8_t *const buf)
{
	LOG_INF("HID OUT: bytes=%u buf[0]=0x%02x", len, len > 0 ? buf[0] : 0xFF);

	if (len == 4 && buf[0] == HID_REPORT_ID_LED) {
		memcpy(led_out_buf, buf, 4);
	} else if (len == 3) {
		led_out_buf[0] = HID_REPORT_ID_LED;
		memcpy(&led_out_buf[1], buf, 3);
	} else {
		LOG_WRN("HID OUT: unexpected length %u, ignored", len);
		return -EINVAL;
	}

	k_work_submit(&led_apply_work);
	return 0;
}

/* ── Input report deferred work ───────────────────────────────────────────── */
static void hid_in_fn(struct k_work *w)
{
	ARG_UNUSED(w);
	uint8_t mask = (uint8_t)atomic_get(&hid_win_mask);
	int ret;

	if (!atomic_get(&hid_ready)) {
		return;
	}

	if (atomic_set(&hid_in_busy, 1)) {
		/* Previous report still in flight, input_report_done resubmits */
		return;
	}

	hid_in_report[0] = HID_REPORT_ID_BUTTONS;
	hid_in_report[1] = mask;

	ret = hid_device_submit_report(hid_dev, HID_IN_REPORT_SIZE, hid_in_report);
	if (ret) {
		atomic_clear(&hid_in_busy);
		LOG_WRN("HID send failed: %d", ret);
	} else {
		LOG_DBG("HID win_mask=0x%02x", mask);
	}
}

static K_WORK_DEFINE(hid_in_work, hid_in_fn);

/* ── HID device callbacks ─────────────────────────────────────────────────── */
static void hid_iface_ready(const struct device *dev, const bool ready)
{
	ARG_UNUSED(dev);
	LOG_INF("HID interface %s", ready ? "ready" : "not ready");
	atomic_set(&hid_ready, ready);
	atomic_clear(&hid_in_busy);
}

static int hid_get_report(const struct device *dev, const uint8_t type,
			  const uint8_t id, const uint16_t len,
			  uint8_t *const buf)
{
	ARG_UNUSED(dev);

	if (type != HID_REPORT_TYPE_INPUT || id != HID_REPORT_ID_BUTTONS ||
	    len < HID_IN_REPORT_SIZE) {
		return -ENOTSUP;
	}

	buf[0] = HID_REPORT_ID_BUTTONS;
	buf[1] = (uint8_t)atomic_get(&hid_win_mask);

	return HID_IN_REPORT_SIZE;
}

static int hid_set_report(const struct device *dev, const uint8_t type,
			  const uint8_t id, const uint16_t len,
			  const uint8_t *const buf)
{
	ARG_UNUSED(dev);

	if (type != HID_REPORT_TYPE_OUTPUT || id != HID_REPORT_ID_LED) {
		return -ENOTSUP;
	}

	return hid_handle_led_report(len, buf);
}

static void hid_output_report(const struct device *dev, const uint16_t len,
			      const uint8_t *const buf)
{
	ARG_UNUSED(dev);
	(void)hid_handle_led_report(len, buf);
}

static void hid_input_report_done(const struct device *dev,
				  const uint8_t *const report)
{
	ARG_UNUSED(dev);

	atomic_clear(&hid_in_busy);
	/* Send again if the button state changed while the report was in flight */
	if (report[1] != (uint8_t)atomic_get(&hid_win_mask)) {
		k_work_submit(&hid_in_work);
	}
}

static const struct hid_device_ops hid_ops = {
	.iface_ready       = hid_iface_ready,
	.get_report        = hid_get_report,
	.set_report        = hid_set_report,
	.output_report     = hid_output_report,
	.input_report_done = hid_input_report_done,
};

/* ── Public API ───────────────────────────────────────────────────────────── */
void app_usb_hid_init(void)
{
	int ret;

	if (!device_is_ready(hid_dev)) {
		LOG_ERR("HID device not ready");
		return;
	}

	/* Must be registered before usbd_init() (see usbd_init.c) */
	ret = hid_device_register(hid_dev, hid_report_desc,
				  sizeof(hid_report_desc), &hid_ops);
	if (ret) {
		LOG_ERR("hid_device_register failed: %d", ret);
		return;
	}

	LOG_INF("usb hid init done");
}

/* win_mask: bit 0 = window 1, bit 1 = window 2, bit 2 = window 3 */
void app_usb_hid_send_win_key(uint8_t win_mask, bool pressed)
{
	atomic_set(&hid_win_mask, pressed ? (win_mask & 0x07u) : 0x00u);
	k_work_submit(&hid_in_work);
}
