/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <config.h>
#include <key_handle.h>

#define HID_MAIN_VAL_DATA (0x00 << 0)
#define HID_MAIN_VAL_CONST (0x01 << 0)

#define HID_MAIN_VAL_ARRAY (0x00 << 1)
#define HID_MAIN_VAL_VAR (0x01 << 1)

#define HID_MAIN_VAL_ABS (0x00 << 2)
#define HID_MAIN_VAL_REL (0x01 << 2)

#define HID_MAIN_VAL_NO_WRAP (0x00 << 3)
#define HID_MAIN_VAL_WRAP (0x01 << 3)

#define HID_MAIN_VAL_LIN (0x00 << 4)
#define HID_MAIN_VAL_NON_LIN (0x01 << 4)

#define HID_MAIN_VAL_PREFERRED (0x00 << 5)
#define HID_MAIN_VAL_NO_PREFERRED (0x01 << 5)

#define HID_MAIN_VAL_NO_NULL (0x00 << 6)
#define HID_MAIN_VAL_NULL (0x01 << 6)

#define HID_MAIN_VAL_NON_VOL (0x00 << 7)
#define HID_MAIN_VAL_VOL (0x01 << 7)

#define HID_MAIN_VAL_BIT_FIELD (0x00 << 8)
#define HID_MAIN_VAL_BUFFERED_BYTES (0x01 << 8)


#define HID_REPORT_ID_KEYBOARD 0x01
#define HID_REPORT_ID_CONSUMER 0x02
#define HID_REPORT_ID_VOICE 0x03

#define HID_USAGE_CONSUMER_CONSUMER_CONTROL (0x01)      
#define HID_USAGE_GD_KEYBOARD (0x06)

/** @brief  HIDS Report Id Definition. */
#define HIDS_KB_REPORT_ID                      1
#if FEATURE_SUPPORT_MULTIMEDIA_KEYBOARD
#define HIDS_MM_KB_REPORT_ID                   2
#endif
#if (VOICE_ENC_TYPE == SW_MSBC_ENC)
#define HIDS_VOICE_REPORT_ID                0x5A // input and output
#elif (VOICE_ENC_TYPE == SW_SBC_ENC)
#define HIDS_VOICE_REPORT_ID                0x5B // input and output
#else
#define HIDS_VOICE_REPORT_ID                0x5A // input and output
#endif

static const uint8_t hid_report_desc[] = {

	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
    HID_USAGE(HID_USAGE_GD_KEYBOARD),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(HID_REPORT_ID_KEYBOARD),

    HID_USAGE_PAGE(HID_USAGE_KEY),
    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x00),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX8(0xFF),
    HID_REPORT_SIZE(0x08),
    HID_REPORT_COUNT(0x08),
    HID_INPUT(HID_MAIN_VAL_DATA | HID_MAIN_VAL_ARRAY | HID_MAIN_VAL_ABS),

    HID_END_COLLECTION,
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),
    HID_USAGE(HID_USAGE_CONSUMER_CONSUMER_CONTROL),
    HID_COLLECTION(HID_COLLECTION_APPLICATION),
    HID_REPORT_ID(HID_REPORT_ID_CONSUMER),
    HID_USAGE_PAGE(HID_USAGE_CONSUMER),

    HID_LOGICAL_MIN8(0x00),
    HID_LOGICAL_MAX16(0xFF, 0x0F),
    HID_USAGE_MIN8(0x00),
    HID_USAGE_MAX16(0xFF, 0x0F),
    HID_REPORT_SIZE(0x10),
    HID_REPORT_COUNT(0x03),
    HID_INPUT(HID_MAIN_VAL_DATA | HID_MAIN_VAL_ARRAY | HID_MAIN_VAL_ABS),
    HID_END_COLLECTION,

#if SUPPORT_VOICE_FEATURE
    0x06, 0x00, 0xff,          /* USAGE_PAGE (vendor define) */
    0x09, 0x00,                /* USAGE (Undefine) */
    0xa1, 0x01,  /* COLLECTION (Application) */
    0x85, HIDS_VOICE_REPORT_ID,  /* REPORT_ID (RMC_VENDOR_REPORT_ID_1) */
    0x95, 0xff,  /* Report Count */
    0x75, 0x08,  /* Report Size */
    0x15, 0x00,  /* Logical Minimum */
    0x25, 0xff,  /* Logical Maximum */
    0x19, 0x00,  /* Usage Minimum */
    0x29, 0xff,  /* Usage Maximun */
    0x81, 0x00,  /* Input */
    HID_END_COLLECTION,
#endif
};