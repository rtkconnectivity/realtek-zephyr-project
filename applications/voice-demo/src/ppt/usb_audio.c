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
#include <zephyr/drivers/uart.h>
#include <string.h>

#include <config.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

#if FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA
static const struct device *uac_dump_uart;
#endif

/*
 * Zephyr USB Audio class configured for CONFIG_APP_UAC_SAMPLE_RATE output.
 * Master encodes at that rate, 16-bit, mono.
 * USB frame size = bytes-per-sample * channels * samples-per-ms.
 *
 * 2.4G RF delivers 2 frames per 15ms burst but with irregular intra-burst
 * timing (e.g. frame-1 at t=4ms, frame-2 at t=10ms within the period).
 * RING_BUF_PREFILL_BYTES ensures the ring buffer holds at least one full
 * 15ms burst before USB playback begins, absorbing that jitter.
 * REPRIMING_THRESHOLD detects stream-stop so the gate resets automatically.
 */
#define USB_FRAME_BYTES         ((CONFIG_APP_UAC_SAMPLE_RATE / 1000) * 2u)
#define RING_BUF_SIZE           (USB_FRAME_BYTES * 45u)   /* 1440 bytes ≈ 45ms */
#define RING_BUF_PREFILL_BYTES  (USB_FRAME_BYTES * 15u)   /* 480 bytes = one 15ms burst */
#define REPRIMING_THRESHOLD     50u                        /* 50 consecutive empty frames → re-prime */

RING_BUF_DECLARE(mic_rb, RING_BUF_SIZE);
NET_BUF_POOL_FIXED_DEFINE(mic_pool, 5, USB_FRAME_BYTES, 4, NULL);

static const struct device *mic_dev;
static struct k_work_delayable usb_prime_work;
static bool     mic_primed;
static uint32_t empty_frame_count;

static void data_request_cb(const struct device *dev)
{
	static uint32_t sof_count;
	static bool first_call = true;
	struct net_buf *buf;
	uint32_t claimed;
	uint8_t tmp[USB_FRAME_BYTES];
	int ret;

	if (first_call) {
		LOG_INF("usb audio: data_request_cb first call, streaming active");
		first_call = false;
	}

	/*
	 * Pre-fill gate: hold off real audio until the ring buffer has accumulated
	 * enough data to survive the worst-case intra-burst gap from 2.4G RF.
	 * During the wait, send silence so the USB transfer chain stays alive.
	 */
	if (!mic_primed) {
		if (ring_buf_size_get(&mic_rb) < RING_BUF_PREFILL_BYTES) {
			memset(tmp, 0, USB_FRAME_BYTES);
			goto queue_buf;
		}
		mic_primed = true;
		empty_frame_count = 0;
		LOG_INF("usb audio: pre-fill complete rb=%u, starting real playback",
			ring_buf_size_get(&mic_rb));
	}

	claimed = ring_buf_get(&mic_rb, tmp, USB_FRAME_BYTES);
	if (claimed < USB_FRAME_BYTES) {
		memset(tmp + claimed, 0, USB_FRAME_BYTES - claimed);
		if (claimed > 0) {
			LOG_INF("usb audio: underrun got=%u pad=%u", claimed,
				USB_FRAME_BYTES - claimed);
		}
	}

	/* Detect stream-stop: N consecutive all-zero frames → reset pre-fill gate */
	if (claimed == 0) {
		if (++empty_frame_count >= REPRIMING_THRESHOLD) {
			mic_primed = false;
			empty_frame_count = 0;
			LOG_INF("usb audio: stream stopped, resetting pre-fill gate");
		}
	} else {
		empty_frame_count = 0;
	}

queue_buf:
	buf = net_buf_alloc(&mic_pool, K_NO_WAIT);
	if (!buf) {
		LOG_WRN("usb audio: no net_buf");
		return;
	}
	net_buf_reset(buf);
	net_buf_add_mem(buf, tmp, USB_FRAME_BYTES);
	// LOG_DBG("USB audio send voice data sof=%u", sof_count);

#if FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA
	/* Dump every 10th frame to keep UART output below ~9600 B/s (fits 115200 baud). */
	if (uac_dump_uart != NULL && (sof_count % 10 == 0)) {
		for (int i = 0; i < USB_FRAME_BYTES; i++) {
			uart_poll_out(uac_dump_uart, tmp[i]);
		}
	}
#endif

	ret = usb_audio_send(dev, buf, USB_FRAME_BYTES);
	if (ret != 0) {
		net_buf_unref(buf);
		if (ret == -EAGAIN) {
			/* Host switched to passive interface (tx_enable=false).
			 * Stop driving the transfer chain — it will be restarted
			 * by data_request_cb when the host re-opens alt=1.
			 */
			LOG_DBG("usb audio: passive interface, stopping (sof=%u)", sof_count);
			return;
		}
		/* Transient transfer error — keep the chain alive. */
		LOG_WRN("usb audio: usb_audio_send ret=%d sof=%u", ret, sof_count);
		k_work_reschedule(&usb_prime_work, K_NO_WAIT);
		return;
	}

	sof_count++;
	if (sof_count % 600 == 0) {
		LOG_INF("usb audio: sent %u SOF frames, rb_free=%u",
			sof_count, ring_buf_space_get(&mic_rb));
	}
}

static void data_written_cb(const struct device *dev, struct net_buf *buf, size_t size)
{
	net_buf_unref(buf);
	// LOG_DBG("XFER_COMPL: size=%d", size);
	/* RTL87x2G doesn't fire USB_DC_SOF, drive the next frame from write-completion. */
	k_work_reschedule(&usb_prime_work, K_NO_WAIT);
}

static const struct usb_audio_ops mic_ops = {
	.data_request_cb = data_request_cb,
	.data_written_cb = data_written_cb,
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

static void usb_prime_handler(struct k_work *work)
{
	if (mic_dev) {
		data_request_cb(mic_dev);
	}
}

static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	LOG_INF("USB status: %s (%d)", usb_status_str(status), (int)status);

	/* On interface change, cancel any pending work so the pump doesn't
	 * keep firing while the host is on the passive (alt=0) interface.
	 * The transfer chain restarts via data_request_cb when alt=1 opens. */
	if (status == USB_DC_INTERFACE) {
		k_work_cancel_delayable(&usb_prime_work);
	}

	/* On disconnect/reset, flush the ring buffer and reset the pre-fill gate
	 * so the next connection starts clean. */
	if (status == USB_DC_DISCONNECTED || status == USB_DC_RESET) {
		k_work_cancel_delayable(&usb_prime_work);
		ring_buf_reset(&mic_rb);
		mic_primed = false;
		empty_frame_count = 0;
	}
}

void app_usb_audio_init(void)
{
	mic_dev = DEVICE_DT_GET_ONE(usb_audio_mic);
	if (!device_is_ready(mic_dev)) {
		LOG_ERR("usb audio mic not ready");
		return;
	}

#if FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA
	uac_dump_uart = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(voice_console));
	if (uac_dump_uart == NULL || !device_is_ready(uac_dump_uart)) {
		LOG_WRN("usb audio: dump uart not ready, UAC dump disabled");
		uac_dump_uart = NULL;
	}
#endif

	k_work_init_delayable(&usb_prime_work, usb_prime_handler);
	usb_audio_register(mic_dev, &mic_ops);
	usb_enable(usb_status_cb);
	LOG_INF("usb audio init done");
}

void app_usb_audio_send(const uint8_t *pcm_data, int pcm_size)
{
	if (!pcm_data || pcm_size <= 0 || (pcm_size % 2 != 0)) {
		LOG_ERR("Invalid PCM size: %d", pcm_size);
		return;
	}

	static uint32_t frame_count;

	ring_buf_put(&mic_rb, pcm_data, (uint32_t)pcm_size);

	frame_count++;
	if (frame_count <= 50 || frame_count % 100 == 0) {
		LOG_INF("usb audio: push pcm=%d rb_used=%u frames=%u",
			pcm_size, ring_buf_size_get(&mic_rb), frame_count);
	}
}
