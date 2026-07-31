/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * NOTE: This file requires libppt_sync_slave.a (not yet distributed).
 *       Enable CONFIG_VOICE_PPT_SLAVE only when the slave library is available.
 *       The USB audio output function app_usb_audio_send() must be implemented
 *       in the USB layer (declared in inc/ppt/voice_ppt_slave.h).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#include <config.h>
#include <ppt_sync.h>
#include <voice/sbc.h>
#include <ppt/ppt_protocol.h>
#include <ppt/voice_ppt_slave.h>
#include "rtl_pinmux.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

T_VOICE_PPT_SLAVE_DATA voice_ppt_slave_data;
uint16_t slave_pair_time_cnt = 0;

#if FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA
static const struct device *dump_uart;
#endif

static void ppt_slave_receive_cb(uint8_t *data, uint16_t len, sync_receive_info_t *info)
{
	if (len < VOICE_PPT_PACKET_SIZE) {
		LOG_DBG("ppt slave: short packet len=%d", len);
		return;
	}

	if (data[VOICE_PPT_OFFSET_OPCODE] != SYNC_OPCODE_VOICE) {
		return;
	}

	LOG_DBG("ppt_slave_receive_cb: voice data received!");

	int ret;

#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
	/*
	 * mSBC frame layout: [0x01, seq, sbc_data[57], 0x00]
	 * Raw SBC data starts at VOICE_MSBC_SBC_OFFSET (2) within the frame.
	 */
	const uint8_t *msbc_frame = &data[VOICE_PPT_OFFSET_MSBC];
	uint8_t pcm_buf[240];  /* mSBC: 240 bytes PCM per frame at 16kHz */
	int pcm_size = (int)sizeof(pcm_buf);

	ret = sbc_decode((unsigned char *)(msbc_frame + VOICE_MSBC_SBC_OFFSET),
			 VOICE_MSBC_SBC_SIZE, pcm_buf, &pcm_size);

#elif (VOICE_ENC_TYPE == SW_SBC_ENC)
	/* Raw SBC bitstream, no framing header */
	const uint8_t *sbc_frame = &data[VOICE_PPT_OFFSET_SBC];
	uint8_t pcm_buf[256];  /* SBC bitpool=14: 16 blocks × 8 subbands × 2B = 256 bytes */
	int pcm_size = (int)sizeof(pcm_buf);

	ret = sbc_decode((unsigned char *)sbc_frame, VOICE_SBC_FRAME_SIZE,
			 pcm_buf, &pcm_size);
#endif /* VOICE_ENC_TYPE */

	if (ret < 0) {
		LOG_DBG("ppt slave: sbc_decode failed ret=%d frame_idx=%d",
			ret, data[VOICE_PPT_OFFSET_FRAME_IDX]);
		return;
	}

	LOG_DBG("ppt slave: sbc_decode ok consumed=%d pcm_size=%d frame_idx=%d",
		ret, pcm_size, data[VOICE_PPT_OFFSET_FRAME_IDX]);

#if FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA
	if (dump_uart != NULL) {
		for (int i = 0; i < pcm_size; i++) {
			uart_poll_out(dump_uart, pcm_buf[i]);
		}
	}
#endif
	Pad_Config(P0_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                   PAD_OUT_HIGH);
	Pad_Config(P0_0, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_DOWN, PAD_OUT_ENABLE,
                   PAD_OUT_LOW);
	app_usb_audio_send(pcm_buf, pcm_size);
}

static void ppt_slave_event_cb(sync_event_t event)
{
	switch (event) {
	case SYNC_EVENT_PAIRED:
		LOG_DBG("ppt slave: paired");
		voice_ppt_slave_data.is_bonded = true;
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_CONNECTED;
		break;

	case SYNC_EVENT_PAIR_TIMEOUT:
		LOG_DBG("ppt slave: pair timeout");
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_IDLE;
		sync_pair();
		break;

	case SYNC_EVENT_CONNECTED:
		LOG_DBG("ppt slave: connected");
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_CONNECTED;
		break;

	case SYNC_EVENT_CONNECT_TIMEOUT:
		LOG_DBG("ppt slave: connect timeout");
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_IDLE;
		break;

	case SYNC_EVENT_CONNECT_LOST:
		LOG_DBG("ppt slave: connect lost");
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_DISCONNECTED;
		break;

	default:
		break;
	}
}

void voice_ppt_slave_init(void)
{
	memset(&voice_ppt_slave_data, 0, sizeof(voice_ppt_slave_data));
	voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_IDLE;

#if FEATURE_SUPPORT_UART_DUMP_VOICE_DECODE_DATA
	dump_uart = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(voice_console));
	if (dump_uart == NULL || !device_is_ready(dump_uart)) {
		LOG_WRN("ppt slave: dump uart not ready, PCM dump disabled");
		dump_uart = NULL;
	}
#endif

	sync_init(SYNC_ROLE_SLAVE);
	sync_event_cb_reg(ppt_slave_event_cb);
	sync_msg_reg_receive_cb(ppt_slave_receive_cb);

	sync_pair_rssi_set(-65);

    // /*set crc 8*/
    // sync_crc_set(8, 0x07, 0xff);

    uint8_t msg_quota[SYNC_MSG_TYPE_NUM] = {0, 3, 3, 3};
    sync_msg_set_quota(msg_quota);

    sync_log_set(0, true);

	sbc_init_decoder();
	LOG_DBG("ppt slave init done");
}

void voice_ppt_slave_enable(void)
{
	sync_bond_info_t bond_info;

	sync_enable();

	if (sync_nvm_get_bond_info(&bond_info) == SYNC_ERR_CODE_SUCCESS) {
		voice_ppt_slave_data.is_bonded = true;
		LOG_DBG("ppt slave: bond found, reconnecting");
		sync_connect(&bond_info);
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_DISCONNECTED;
	} else {
		LOG_DBG("ppt slave: no bond, starting pairing");
		sync_pair();
		voice_ppt_slave_data.state = VOICE_PPT_SLAVE_STATE_PAIRING;
	}
}
