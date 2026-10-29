/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Application-level configuration for claude_micro.
 * No encoding — raw PCM is sent directly to USB UAC.
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/* ── Voice flow ───────────────────────────────────────────────────────────── */
/* Raw PCM path: no BLE, no PPT, no encoding. */
#define VOICE_FLOW_RAW_USB      1

/* ── Codec sample rate ────────────────────────────────────────────────────── */
/* Derived from Kconfig choice; just expose a convenience alias. */
#define CODEC_SAMPLE_RATE       CONFIG_APP_UAC_SAMPLE_RATE

/* ── I2S block size ───────────────────────────────────────────────────────── */
/*
 * 10 ms worth of 16-bit mono PCM at the configured sample rate.
 *   16 kHz → 320 bytes
 *   48 kHz → 960 bytes
 */
#define VOICE_BLOCK_SIZE        ((CONFIG_APP_UAC_SAMPLE_RATE / 1000) * 10 * 2)

/* ── Debug / dump features ────────────────────────────────────────────────── */
/* Set to 1 to dump raw PCM bytes over UART2 (expensive — debug only). */
#define FEATURE_SUPPORT_UART_DUMP_VOICE_RAW_DATA    0

/* Set to 1 to dump every 10th UAC frame over UART2 (debug only). */
#define FEATURE_SUPPORT_UART_DUMP_UAC_SEND_DATA     0

/* ── HID report IDs ───────────────────────────────────────────────────────── */
#define HID_REPORT_ID_BUTTONS   0x01   /* IN:  3-bit window YES buttons        */
#define HID_REPORT_ID_LED       0x02   /* OUT: per-window LED state from host  */

/* HID key code sent when the mic button is held (consumer: "Record"). */
#define HID_KEY_RECORD          0xB3

/* ── LED window state values (used in HID OUT report and led_driver) ─────── */
#define LED_WIN_IDLE            0U   /* off                     */
#define LED_WIN_WORKING         1U   /* slow blink — executing  */
#define LED_WIN_WAITING         2U   /* fast blink — waiting    */
#define LED_WIN_SUCCESS         3U   /* solid on   — done       */

#endif /* _CONFIG_H_ */
