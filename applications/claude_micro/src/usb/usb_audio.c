/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB Audio Class 2 (UAC2) microphone for claude_micro (USB device_next).
 *
 * Ring-buffer design:
 *   app_usb_audio_send() → ring buffer → mic_sof_cb() → usbd_uac2_send()
 *
 * Pre-fill gate: silence is sent until RING_BUF_PREFILL_BYTES accumulate,
 * preventing underruns at stream start.
 *
 * Drive loop: one ISO IN frame (1 ms) is queued from every USB SOF while the
 * host has the streaming interface enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/drivers/usb/usb_buf.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <config.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/*
 * USB frame payload = 1 ms of 16-bit mono PCM.
 *   16 kHz →  32 bytes/frame
 *   48 kHz →  96 bytes/frame
 */
#define USB_FRAME_BYTES         ((CONFIG_APP_UAC_SAMPLE_RATE / 1000) * 2u)
#define RING_BUF_SIZE           (USB_FRAME_BYTES * 45u)   /* ~45 ms */
#define RING_BUF_PREFILL_BYTES  (USB_FRAME_BYTES * 15u)   /* 15 ms pre-fill */
#define REPRIMING_THRESHOLD     50u   /* 50 silent frames → reset gate */
#define NUM_USB_BUFS            4u

#define MIC_TERMINAL_ID         UAC2_ENTITY_ID(DT_NODELABEL(in_terminal))

RING_BUF_DECLARE(mic_rb, RING_BUF_SIZE);
K_MEM_SLAB_DEFINE_STATIC(mic_slab, ROUND_UP(USB_FRAME_BYTES, UDC_BUF_GRANULARITY),
			 NUM_USB_BUFS, UDC_BUF_ALIGN);

static const struct device *mic_dev = DEVICE_DT_GET(DT_NODELABEL(uac2_mic));
static atomic_t mic_active;
static bool     mic_primed;
static uint32_t empty_frame_count;

static void mic_stream_reset(void)
{
	ring_buf_reset(&mic_rb);
	mic_primed        = false;
	empty_frame_count = 0;
}

/* ── sof_cb: called by USB stack every 1 ms frame ─────────────────────────── */
static void mic_sof_cb(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uint8_t *buf;
	uint32_t claimed;
	int ret;

	if (!atomic_get(&mic_active)) {
		return;
	}

	if (k_mem_slab_alloc(&mic_slab, (void **)&buf, K_NO_WAIT) != 0) {
		LOG_DBG("usb audio: no tx buffer");
		return;
	}

	/* Pre-fill gate */
	if (!mic_primed) {
		if (ring_buf_size_get(&mic_rb) < RING_BUF_PREFILL_BYTES) {
			memset(buf, 0, USB_FRAME_BYTES);
			goto queue_buf;
		}
		mic_primed = true;
		empty_frame_count = 0;
		LOG_INF("usb audio: pre-fill done rb=%u", ring_buf_size_get(&mic_rb));
	}

	claimed = ring_buf_get(&mic_rb, buf, USB_FRAME_BYTES);
	if (claimed < USB_FRAME_BYTES) {
		memset(buf + claimed, 0, USB_FRAME_BYTES - claimed);
		if (claimed > 0) {
			LOG_DBG("usb audio: underrun got=%u pad=%u",
				claimed, USB_FRAME_BYTES - claimed);
		}
	}

	/* Stream-stop detection */
	if (claimed == 0) {
		if (++empty_frame_count >= REPRIMING_THRESHOLD) {
			mic_primed      = false;
			empty_frame_count = 0;
			LOG_INF("usb audio: stream stopped, gate reset");
		}
	} else {
		empty_frame_count = 0;
	}

queue_buf:
	/* On error the class does not take ownership, free it here.
	 * If the terminal went inactive, buf_release_cb already freed it. */
	ret = usbd_uac2_send(dev, MIC_TERMINAL_ID, buf, USB_FRAME_BYTES);
	if (ret != 0) {
		LOG_DBG("usbd_uac2_send failed: %d", ret);
		k_mem_slab_free(&mic_slab, buf);
	}
}

static void mic_terminal_update_cb(const struct device *dev, uint8_t terminal,
				   bool enabled, bool microframes,
				   void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(microframes);
	ARG_UNUSED(user_data);

	if (terminal != MIC_TERMINAL_ID) {
		return;
	}

	mic_stream_reset();
	atomic_set(&mic_active, enabled);
	LOG_INF("usb audio: streaming %s", enabled ? "enabled" : "disabled");
}

static void mic_buf_release_cb(const struct device *dev, uint8_t terminal,
			       void *buf, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(terminal);
	ARG_UNUSED(user_data);

	k_mem_slab_free(&mic_slab, buf);
}

static const struct uac2_ops mic_ops = {
	.sof_cb             = mic_sof_cb,
	.terminal_update_cb = mic_terminal_update_cb,
	.buf_release_cb     = mic_buf_release_cb,
};

/* ── Public API ───────────────────────────────────────────────────────────── */
void app_usb_audio_init(void)
{
	if (!device_is_ready(mic_dev)) {
		LOG_ERR("usb audio mic not ready");
		return;
	}

	/* Must be registered before usbd_init() (see usbd_init.c) */
	usbd_uac2_set_ops(mic_dev, &mic_ops, NULL);
	LOG_INF("usb audio init done (frame=%u bytes)", USB_FRAME_BYTES);
}

void app_usb_audio_send(const uint8_t *pcm_data, int pcm_size)
{
	if (!pcm_data || pcm_size <= 0) {
		return;
	}
	ring_buf_put(&mic_rb, pcm_data, (uint32_t)pcm_size);
}
