/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <config.h>

/*
 * Voice-over-2.4G packet layout, selected by VOICE_ENC_TYPE:
 *
 * mSBC (SW_MSBC_ENC), 62 bytes:
 *   [0]     opcode    = SYNC_OPCODE_VOICE (0x15)
 *   [1]     frame_idx (0 = first frame of DMA block, 1 = second)
 *   [2..61] mSBC frame (60 bytes): [0x01, seq, 57-byte SBC data, 0x00]
 *   decode: sbc_decode(&pkt[4], 57, pcm_buf, &pcm_size)
 *
 * SBC (SW_SBC_ENC), 38 bytes:
 *   [0]     opcode    = SYNC_OPCODE_VOICE (0x15)
 *   [1]     frame_idx (0 = first frame of DMA block, 1 = second)
 *   [2..37] raw SBC bitstream (36 bytes, bitpool=14)
 *   decode: sbc_decode(&pkt[2], 36, pcm_buf, &pcm_size)
 */

#define SYNC_OPCODE_VOICE          0x15u

#define VOICE_PPT_OFFSET_OPCODE    0u
#define VOICE_PPT_OFFSET_FRAME_IDX 1u

#if (VOICE_ENC_TYPE == SW_MSBC_ENC)

#define VOICE_MSBC_FRAME_SIZE      60u
#define VOICE_PPT_PACKET_SIZE      (1u + 1u + VOICE_MSBC_FRAME_SIZE)  /* 62 bytes */
#define VOICE_PPT_OFFSET_MSBC      2u
/* Offset inside a 60-byte mSBC frame where raw SBC data begins */
#define VOICE_MSBC_SBC_OFFSET      2u   /* skip 0x01 sync + seq byte */
#define VOICE_MSBC_SBC_SIZE        57u

#elif (VOICE_ENC_TYPE == SW_SBC_ENC)

/* SBC frame size for bitpool=14: 2*14 + 8 = 36 bytes */
#define VOICE_SBC_FRAME_SIZE       36u
#define VOICE_PPT_PACKET_SIZE      (1u + 1u + VOICE_SBC_FRAME_SIZE)   /* 38 bytes */
#define VOICE_PPT_OFFSET_SBC       2u

#endif /* VOICE_ENC_TYPE */
