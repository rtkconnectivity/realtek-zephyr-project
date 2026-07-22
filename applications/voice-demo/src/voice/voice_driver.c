/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>
#include <voice/voice_driver.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/audio/codec.h>
#include <config.h>
#include "trace.h"
#include "rtl_pinmux.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

const struct device *dev_uart = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(voice_console));
const struct device *dev_i2s = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(i2s0));
const struct device *dev_codec = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(codec));

static struct audio_codec_cfg codec_cfg;

T_VOICE_DRIVER_GLOBAL_DATA voice_driver_global_data;

#define BLOCK_SIZE    480
#define NUM_RX_BLOCKS 2
#define MAX_VOICE_TIMEOUT_SEC	2

K_MEM_SLAB_DEFINE(rx_mem_slab, BLOCK_SIZE, NUM_RX_BLOCKS, 32);
// K_MEM_SLAB_DEFINE(tx_mem_slab, BLOCK_SIZE, NUM_RX_BLOCKS, 32);

static void voice_timer_timeout(struct k_timer *timer);
K_TIMER_DEFINE(voice_timer, voice_timer_timeout, NULL);

static void voice_timer_timeout(struct k_timer *timer)
{
	LOG_DBG("voice_timer_timeout");
	k_timer_stop(timer);
	voice_driver_global_data.is_voice_stop_rx_data = true;
	/* stop voice recording immediately */
	voice_handle_stop_mic();
}

static void voice_driver_init_codec_i2s(void)
{
	int ret;
	LOG_DBG("voice_driver_init_codec_i2s");
	audio_property_t property = AUDIO_PROPERTY_OUTPUT_MUTE;
	audio_channel_t channel = AUDIO_CHANNEL_ALL;
	audio_property_value_t val = {
		.vol = 0,
		.mute = false,
	};

	memset(&codec_cfg, 0, sizeof(struct audio_codec_cfg));
	codec_cfg.mclk_freq = 2500000;
	codec_cfg.dai_type = AUDIO_DAI_TYPE_I2S;
	codec_cfg.dai_cfg.i2s.frame_clk_freq = 16000;
	codec_cfg.dai_cfg.i2s.word_size = 16;
	codec_cfg.dai_cfg.i2s.channels = 1;
	codec_cfg.dai_cfg.i2s.format = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_DATA_ORDER_MSB;
	codec_cfg.dai_cfg.i2s.options = I2S_OPT_BIT_CLK_CONT | I2S_OPT_BIT_CLK_MASTER |
					I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_PINGPONG;

	codec_cfg.dai_cfg.i2s.block_size = BLOCK_SIZE;
	codec_cfg.dai_cfg.i2s.mem_slab = &rx_mem_slab;
	codec_cfg.dai_cfg.i2s.timeout = SYS_FOREVER_MS;

	ret = audio_codec_configure(dev_codec, &codec_cfg);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}

	ret = audio_codec_set_property(dev_codec, property, channel, val);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}

	ret = audio_codec_apply_properties(dev_codec);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}

	/* init codec pin + init codec reg */
	audio_codec_start_output(dev_codec);

	/* enable i2s rcc + init i2s reg */
	codec_cfg.dai_cfg.i2s.frame_clk_freq = 16000;
	ret = i2s_configure(dev_i2s, I2S_DIR_RX, &codec_cfg.dai_cfg.i2s);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}
}

static void voice_driver_start(void)
{
	LOG_DBG("voice_driver_start");
	k_timer_start(&voice_timer, K_SECONDS(MAX_VOICE_TIMEOUT_SEC), K_NO_WAIT);
	/* enable i2s + config dma + enable dma */
	int ret = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}
}

void voice_driver_init(void)
{
	voice_driver_init_codec_i2s();
	memset(&voice_driver_global_data, 0, sizeof(voice_driver_global_data));
	voice_driver_global_data.is_voice_driver_working = true;
	voice_driver_start();
}

static void voice_driver_deinit_codec(void)
{
	/* deinit codec */
	audio_codec_stop_output(dev_codec);
}

static void voice_driver_deinit_i2s_gdma(void)
{
	/* disable i2s + disable dma */
	int ret = i2s_trigger(dev_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
	if (ret) {
		LOG_DBG("[%s] ret%d line%d\n", __func__, ret, __LINE__);
	}

	/* disable i2s rcc */
	codec_cfg.dai_cfg.i2s.frame_clk_freq = 0;
	ret = i2s_configure(dev_i2s, I2S_DIR_RX, &codec_cfg.dai_cfg.i2s);
	if (ret) {
		LOG_DBG("ret%d line%d\n", ret, __LINE__);
	}
}

void voice_driver_deinit(void)
{
	voice_driver_deinit_i2s_gdma();
	voice_driver_deinit_codec();
	voice_driver_global_data.is_voice_driver_working = false;
	voice_driver_global_data.is_voice_stop_rx_data = true;
}