/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <ppt_sync.h>
#include <ppt/ppt_protocol.h>
#include <ppt/voice_ppt_master.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

T_VOICE_PPT_MASTER_DATA voice_ppt_master_data;
uint16_t master_pair_time_cnt = 0;

static void ppt_master_send_cb(sync_msg_type_t type, uint8_t *data, uint16_t len,
				sync_send_info_t *info)
{
	if (info->res != SYNC_SEND_RESULT_ACKED) {
		LOG_DBG("ppt master send nacked/lost, retrans=%d", info->retrans_count);
	}
}

static void ppt_master_event_cb(sync_event_t event)
{
	switch (event) {
	case SYNC_EVENT_PAIRED:
		LOG_DBG("ppt master: paired");
		voice_ppt_master_data.is_bonded = true;
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_CONNECTED;
		break;

	case SYNC_EVENT_PAIR_TIMEOUT:
		LOG_DBG("ppt master: pair timeout");
		if(master_pair_time_cnt < PPT_PAIR_TIME_MAX_COUNT) {
			master_pair_time_cnt++;
			sync_pair();
		} else {
			master_pair_time_cnt = 0;
			voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_IDLE;
		}
		break;

	case SYNC_EVENT_CONNECTED:
		LOG_DBG("ppt master: connected");
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_CONNECTED;
		break;

	case SYNC_EVENT_CONNECT_TIMEOUT:
		LOG_DBG("ppt master: connect timeout");
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_IDLE;
		break;

	case SYNC_EVENT_CONNECT_LOST:
		LOG_DBG("ppt master: connect lost");
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_DISCONNECTED;
		break;

	default:
		break;
	}
}

void voice_ppt_master_init(void)
{
	memset(&voice_ppt_master_data, 0, sizeof(voice_ppt_master_data));
	voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_IDLE;

	sync_init(SYNC_ROLE_MASTER);
	sync_event_cb_reg(ppt_master_event_cb);

	sync_pair_rssi_set(PPT_PAIR_RSSI);

    /* set 2.4g connection heart beat interval */
    sync_master_set_hb_param(2, 250000, 0);

    /*Different message types have different queue size*/
    uint8_t msg_quota[SYNC_MSG_TYPE_NUM] = {0, 3, 3, 3};
    sync_msg_set_quota(msg_quota);
	sync_log_set(0, true);
	LOG_DBG("ppt master init done");
}

void voice_ppt_master_enable(void)
{
	sync_bond_info_t bond_info;

	sync_enable();

	if (sync_nvm_get_bond_info(&bond_info) == SYNC_ERR_CODE_SUCCESS) {
		voice_ppt_master_data.is_bonded = true;
		LOG_DBG("ppt master: bond found, reconnecting");
		sync_connect(&bond_info);
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_DISCONNECTED;
	} else {
		LOG_DBG("ppt master: no bond, starting pairing");
		sync_pair();
		voice_ppt_master_data.state = VOICE_PPT_MASTER_STATE_PAIRING;
	}
}

void app_ppt_send_voice_data(const uint8_t *msbc_frame_pair)
{
	// if (voice_ppt_master_data.state != VOICE_PPT_MASTER_STATE_CONNECTED) {
	// 	return;
	// }

	uint8_t pkt[VOICE_PPT_PACKET_SIZE];

	/* Send first mSBC frame (bytes 0..59) */
	pkt[VOICE_PPT_OFFSET_OPCODE]    = SYNC_OPCODE_VOICE;
	pkt[VOICE_PPT_OFFSET_FRAME_IDX] = 0;
	memcpy(&pkt[VOICE_PPT_OFFSET_MSBC], msbc_frame_pair, VOICE_MSBC_FRAME_SIZE);
	sync_msg_send(SYNC_MSG_TYPE_ONESHOT, pkt, VOICE_PPT_PACKET_SIZE, ppt_master_send_cb);
	LOG_HEXDUMP_DBG(pkt, VOICE_PPT_PACKET_SIZE, "ppt send voice data pk1");

	/* Send second mSBC frame (bytes 60..119) */
	pkt[VOICE_PPT_OFFSET_FRAME_IDX] = 1;
	memcpy(&pkt[VOICE_PPT_OFFSET_MSBC], msbc_frame_pair + VOICE_MSBC_FRAME_SIZE,
	       VOICE_MSBC_FRAME_SIZE);
	sync_msg_send(SYNC_MSG_TYPE_ONESHOT, pkt, VOICE_PPT_PACKET_SIZE, ppt_master_send_cb);
	LOG_HEXDUMP_DBG(pkt, VOICE_PPT_PACKET_SIZE, "ppt send voice data pk2");
}
