/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * I2S / codec driver for claude_micro.
 *
 * BLOCK_SIZE is derived from the Kconfig sample rate so the same source
 * compiles correctly for both 16 kHz and 48 kHz:
 *   16 kHz → 10 ms × 16 samples/ms × 2 bytes = 320 bytes
 *   48 kHz → 10 ms × 48 samples/ms × 2 bytes = 960 bytes
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/audio/codec.h>
#include <zephyr/logging/log.h>
#include <config.h>
#include <voice/voice_driver.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/* ── Devices ──────────────────────────────────────────────────────────────── */
const struct device *dev_i2s   = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(i2s0));
const struct device *dev_codec = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(codec));

/* ── I2S memory slab ──────────────────────────────────────────────────────── */
#define BLOCK_SIZE    VOICE_BLOCK_SIZE   /* from config.h */
/* Need ≥ 2 to start DMA.  Extra blocks absorb the window where DMA callback
 * fires before the app thread has called i2s_read() to free the previous
 * block; without headroom the driver logs "buffer alloc from slab err -12"
 * and enters I2S_STATE_ERROR mid-recording. */
#define NUM_RX_BLOCKS 4

K_MEM_SLAB_DEFINE(rx_mem_slab, BLOCK_SIZE, NUM_RX_BLOCKS, 32);

/* ── Timeout watchdog ─────────────────────────────────────────────────────── */
#define MAX_VOICE_TIMEOUT_SEC   60   /* auto-stop after 60 s */

static void voice_timer_timeout(struct k_timer *timer);
K_TIMER_DEFINE(voice_timer, voice_timer_timeout, NULL);

/* ── Global state (shared with voice_handle.c) ────────────────────────────── */
T_VOICE_DRIVER_GLOBAL_DATA voice_driver_global_data;

/* ── Codec config cache (needed by deinit) ────────────────────────────────── */
static struct audio_codec_cfg codec_cfg;

/* ── Forward ──────────────────────────────────────────────────────────────── */
void voice_handle_stop_mic(void);

static void voice_timer_timeout(struct k_timer *timer)
{
	LOG_WRN("voice timeout — auto-stopping");
	k_timer_stop(timer);
	voice_driver_global_data.is_voice_stop_rx_data = true;
	voice_handle_stop_mic();
}

/* ── Init ─────────────────────────────────────────────────────────────────── */
static void voice_driver_init_codec_i2s(void)
{
	int ret;

	audio_property_t property = AUDIO_PROPERTY_OUTPUT_VOLUME;//AUDIO_PROPERTY_OUTPUT_MUTE;
	audio_channel_t channel   = AUDIO_CHANNEL_ALL;
	audio_property_value_t val = { .vol = 0x2f };

	memset(&codec_cfg, 0, sizeof(codec_cfg));
	codec_cfg.mclk_freq              = 2500000;
	codec_cfg.dai_type               = AUDIO_DAI_TYPE_I2S;
	codec_cfg.dai_cfg.i2s.frame_clk_freq = CONFIG_APP_UAC_SAMPLE_RATE;
	codec_cfg.dai_cfg.i2s.word_size  = 16;
	codec_cfg.dai_cfg.i2s.channels   = 1;
	codec_cfg.dai_cfg.i2s.format     = I2S_FMT_DATA_FORMAT_I2S |
	                                   I2S_FMT_DATA_ORDER_MSB;
	codec_cfg.dai_cfg.i2s.options    = I2S_OPT_BIT_CLK_CONT |
	                                   I2S_OPT_BIT_CLK_CONTROLLER |
	                                   I2S_OPT_FRAME_CLK_CONTROLLER |
	                                   I2S_OPT_PINGPONG;
	codec_cfg.dai_cfg.i2s.block_size = BLOCK_SIZE;
	codec_cfg.dai_cfg.i2s.mem_slab   = &rx_mem_slab;
	/* Use a finite timeout so i2s_read() can return -EAGAIN when the stream
	 * is stopped (DROP clears out_queue but SYS_FOREVER_MS would block
	 * forever).  100 ms is well above the 10 ms block period. */
	codec_cfg.dai_cfg.i2s.timeout    = 100;

	ret = audio_codec_configure(dev_codec, &codec_cfg);
	if (ret) {
		LOG_ERR("codec configure failed: %d", ret);
	}

	ret = audio_codec_set_property(dev_codec, property, channel, val);
	if (ret) {
		LOG_ERR("codec set_property failed: %d", ret);
	}

	ret = audio_codec_apply_properties(dev_codec);
	if (ret) {
		LOG_ERR("codec apply_properties failed: %d", ret);
	}

	/* Init codec pins and registers */
	audio_codec_start_output(dev_codec);

	/* Enable I2S RCC and configure I2S registers */
	ret = i2s_configure(dev_i2s, I2S_DIR_RX, &codec_cfg.dai_cfg.i2s);
	if (ret) {
		LOG_ERR("i2s_configure failed: %d", ret);
	}
}

void voice_driver_init(void)
{
	LOG_INF("voice_driver_init: slab free=%u",
		k_mem_slab_num_free_get(&rx_mem_slab));
	voice_driver_init_codec_i2s();
	memset(&voice_driver_global_data, 0, sizeof(voice_driver_global_data));
	voice_driver_global_data.is_voice_driver_working = true;

	k_timer_start(&voice_timer, K_SECONDS(MAX_VOICE_TIMEOUT_SEC), K_NO_WAIT);

	int ret = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	LOG_INF("i2s_trigger START ret=%d, slab free=%u",
		ret, k_mem_slab_num_free_get(&rx_mem_slab));
	if (ret) {
		LOG_ERR("i2s_trigger START failed: %d", ret);
	}
}

/* ── Deinit ───────────────────────────────────────────────────────────────── */
void voice_driver_deinit(void)
{
	LOG_INF("voice_driver_deinit: slab free=%u",
		k_mem_slab_num_free_get(&rx_mem_slab));
	k_timer_stop(&voice_timer);

	/* Disable I2S DMA */
	int ret = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
	if (ret) {
		LOG_ERR("i2s_trigger DROP failed: %d", ret);
	}

	/* Disable I2S RCC (freq = 0 signals driver to power off) */
	codec_cfg.dai_cfg.i2s.frame_clk_freq = 0;
	ret = i2s_configure(dev_i2s, I2S_DIR_RX, &codec_cfg.dai_cfg.i2s);
	if (ret) {
		LOG_ERR("i2s_configure (power-off) failed: %d", ret);
	}

	audio_codec_stop_output(dev_codec);

	voice_driver_global_data.is_voice_driver_working = false;
	voice_driver_global_data.is_voice_stop_rx_data   = true;
}
