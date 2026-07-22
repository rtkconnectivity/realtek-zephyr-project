/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/*
 * Voice-over-2.4G packet layout (62 bytes):
 *   [0]    opcode  = SYNC_OPCODE_VOICE (0x15)
 *   [1]    frame_index (0 = first frame of DMA block, 1 = second)
 *   [2..61] mSBC frame (60 bytes): [0x01, seq, 57-byte SBC data, 0x00]
 *
 * mSBC frame decode: sbc_decode(&pkt[4], 57, pcm_buf, &pcm_size)
 * (skip pkt[2]=0x01 sync word, pkt[3]=seq byte → raw SBC at pkt[4])
 */

#define SYNC_OPCODE_VOICE         0x15u

#define VOICE_MSBC_FRAME_SIZE     60u   /* 60-byte mSBC frame */
#define VOICE_PPT_PACKET_SIZE     (1u + 1u + VOICE_MSBC_FRAME_SIZE)  /* 62 bytes */

#define VOICE_PPT_OFFSET_OPCODE   0u
#define VOICE_PPT_OFFSET_FRAME_IDX 1u
#define VOICE_PPT_OFFSET_MSBC     2u

/* Offset inside a 60-byte mSBC frame where raw SBC data begins */
#define VOICE_MSBC_SBC_OFFSET     2u   /* skip 0x01 sync + seq byte */
#define VOICE_MSBC_SBC_SIZE       57u
