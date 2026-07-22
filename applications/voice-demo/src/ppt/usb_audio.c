/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/buf.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_audio.h>
#include <string.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/*
 * Zephyr USB Audio class hardcodes 48kHz output.
 * mSBC decoder outputs 16kHz, 16-bit, mono → 240 bytes per frame.
 * Upsample 3× (zero-order hold): 240 bytes → 720 bytes @ 48kHz.
 * USB frame size = (16/8) * 1 * 48 = 96 bytes per 1ms SOF interval.
 * Ring buffer holds ~8 frames of upsampled data.
 */
#define USB_FRAME_BYTES   96u
#define UPSAMPLE_FACTOR   3u
#define RING_BUF_SIZE     (USB_FRAME_BYTES * 8u)

RING_BUF_DECLARE(mic_rb, RING_BUF_SIZE);
NET_BUF_POOL_FIXED_DEFINE(mic_pool, 5, USB_FRAME_BYTES, 4, NULL);

static const struct device *mic_dev;

static void data_request_cb(const struct device *dev)
{
	struct net_buf *buf;
	uint32_t claimed;
	uint8_t tmp[USB_FRAME_BYTES];

	claimed = ring_buf_get(&mic_rb, tmp, USB_FRAME_BYTES);
	if (claimed < USB_FRAME_BYTES) {
		/* not enough data yet — pad with silence */
		memset(tmp + claimed, 0, USB_FRAME_BYTES - claimed);
	}

	buf = net_buf_alloc(&mic_pool, K_NO_WAIT);
	if (!buf) {
		LOG_DBG("usb audio: no buf");
		return;
	}

	net_buf_add_mem(buf, tmp, USB_FRAME_BYTES);
	usb_audio_send(dev, buf, USB_FRAME_BYTES);
}

static const struct usb_audio_ops mic_ops = {
	.data_request_cb = data_request_cb,
};

static const char *usb_status_str(enum usb_dc_status_code status)
{
	switch (status) {
	case USB_DC_ERROR:        return "ERROR";
	case USB_DC_RESET:        return "RESET";
	case USB_DC_CONNECTED:    return "CONNECTED";
	case USB_DC_CONFIGURED:   return "CONFIGURED";
	case USB_DC_DISCONNECTED: return "DISCONNECTED";
	case USB_DC_SUSPEND:      return "SUSPEND";
	case USB_DC_RESUME:       return "RESUME";
	case USB_DC_INTERFACE:    return "INTERFACE";
	case USB_DC_SET_HALT:     return "SET_HALT";
	case USB_DC_CLEAR_HALT:   return "CLEAR_HALT";
	case USB_DC_SOF:          return "SOF";
	default:                  return "UNKNOWN";
	}
}

static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_INF("USB status: %s (%d)", usb_status_str(status), (int)status);
}

void app_usb_audio_init(void)
{
	mic_dev = DEVICE_DT_GET_ONE(usb_audio_mic);
	if (!device_is_ready(mic_dev)) {
		LOG_ERR("usb audio mic not ready");
		return;
	}

	usb_audio_register(mic_dev, &mic_ops);
	usb_enable(usb_status_cb);
	LOG_INF("usb audio init done");
}

void app_usb_audio_send(const uint8_t *pcm_data, int pcm_size)
{
	/* pcm_data: 16kHz 16-bit mono samples (240 bytes = 120 samples) */
	const int16_t *src = (const int16_t *)pcm_data;
	int samples = pcm_size / 2;
	int16_t upsampled[UPSAMPLE_FACTOR];
	int put;

	for (int i = 0; i < samples; i++) {
		upsampled[0] = src[i];
		upsampled[1] = src[i];
		upsampled[2] = src[i];
		put = ring_buf_put(&mic_rb, (uint8_t *)upsampled, sizeof(upsampled));
		if (put < (int)sizeof(upsampled)) {
			/* ring buffer full — drop oldest by consuming ahead */
			uint8_t discard[USB_FRAME_BYTES];
			ring_buf_get(&mic_rb, discard, USB_FRAME_BYTES);
		}
	}
}
