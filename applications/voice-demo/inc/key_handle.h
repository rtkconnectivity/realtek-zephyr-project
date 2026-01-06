/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <config.h>

/*============================================================================*
 *                              Macros
 *============================================================================*/
#define KEY_CODE_TABLE_SIZE KEY_INDEX_ENUM_GUAID
#define KEYPAD_ROW_SIZE 5
#define KEYPAD_COLUMN_SIZE 4
#define MAX_HID_KEYBOARD_USAGE_CNT 8
#define MAX_HID_CONSUMER_USAGE_CNT 3
#define MAX_HID_VOICE_USAGE_CNT 120

#define HID_USAGE_KEY_KEYBOARD_VOICE 0x3e

/* define the bit mask of combine keys */
#define INVALID_COMBINE_KEYS_BIT_MASK               0x0000
#define PAIRING_COMBINE_KEYS_BIT_MASK               0x0001
#define IR_LEARNING_COMBINE_KEYS_BIT_MASK           0x0002
#define HCI_UART_TEST_COMBINE_KEYS_BIT_MASK         0x0004
#define DATA_UART_TEST_COMBINE_KEYS_BIT_MASK        0x0008
#define SINGLE_TONE_TEST_COMBINE_KEYS_BIT_MASK      0x0010
#define FAST_PAIR_1_COMBINE_KEYS_BIT_MASK           0x0020
#define FAST_PAIR_2_COMBINE_KEYS_BIT_MASK           0x0040
#define FAST_PAIR_3_COMBINE_KEYS_BIT_MASK           0x0080
#define FAST_PAIR_4_COMBINE_KEYS_BIT_MASK           0x0100
#define FAST_PAIR_5_COMBINE_KEYS_BIT_MASK           0x0200
#define BUG_REPORT_COMBINE_KEYS_BIT_MASK            0x0400
#define FACTORY_RESET_COMBINE_KEYS_BIT_MASK         0x8000

#define COMBINE_KEYS_DETECT_TIMEOUT         2000  /* 2 sec */
#define FACTORY_RESET_KEYS_DETECT_TIMEOUT   4000  /* 4 sec */
#define BUG_REPOERT_DETECT_TIMEOUT          1000  /* 1 sec */

/*============================================================================*
 *                         Types
 *============================================================================*/
/* define the key types */
typedef enum
{
    KEY_TYPE_NONE       = 0x00,  /* none key type */
    KEY_TYPE_BLE_ONLY   = 0x01,  /* only BLE key type */
    KEY_TYPE_IR_ONLY    = 0x02,  /* only IR key type */
    KEY_TYPE_BLE_OR_IR  = 0x03,  /* BLE or IR key type */
} T_KEY_TYPE_DEF;

/* define the HID usage pages */
typedef enum
{
    HID_USAGE_UNDEFINED  = 0x00,
    HID_USAGE_KEY   = 0x07,
    HID_USAGE_CONSUMER   = 0x0C,
} T_HID_USAGE_PAGES_DEF;

/* define the struct of key code */
typedef struct
{
    T_KEY_TYPE_DEF key_type;
    uint8_t ir_key_code;
    uint8_t hid_usage_page;
    uint32_t hid_usage_id;
} T_KEY_CODE_DEF;

/**
 * @brief  KeyScan FIFO data struct definition.
 */
typedef struct
{
    uint32_t len;               /**< Keyscan state register        */
    struct
    {
        uint16_t column: 5;      /**< Keyscan raw buffer data       */
        uint16_t row: 4;         /**< Keyscan raw buffer data       */
        uint16_t reserved: 7;
    } key[8];
} T_KEYSCAN_FIFO_DATA;

typedef struct
{
    uint8_t keyboard_usage_cnt;
    uint8_t keyboard_usage_buffer[MAX_HID_KEYBOARD_USAGE_CNT];
    uint8_t consumer_usage_cnt;
    uint16_t consumer_usage_buffer[MAX_HID_CONSUMER_USAGE_CNT];
} T_KEY_HID_USAGES_BUFFER;

/* define the key index */
typedef enum
{
    VK_NONE          = 0x00,
    VK_POWER         = 0x01,
    VK_PAGE_UP       = 0x02,
    VK_PAGE_DOWN     = 0x03,
    VK_MENU          = 0x04,
    VK_HOME          = 0x05,
    VK_VOICE         = 0x06,
    VK_ENTER         = 0x07,
    VK_EXIT          = 0x08,
    VK_LEFT          = 0x09,
    VK_RIGHT         = 0x0A,
    VK_UP            = 0x0B,
    VK_DOWN          = 0x0C,
    VK_MOUSE_EN      = 0x0D,
    VK_VOLUME_MUTE   = 0x0E,
    VK_VOLUME_UP     = 0x0F,
    VK_VOLUME_DOWN   = 0x10,
    VK_VOICE_STOP    = 0x11,
    VK_TV_POWER      = 0x12,
    VK_TV_SIGNAL     = 0x13,
    VK_NETFLIX,
    VK_YOUTUBE,
    VK_APP04,

    MM_ScanNext,
    MM_ScanPrevious,
    MM_Stop,
    MM_Play_Pause,
    MM_Mute,
    MM_BassBoost,
    MM_Loudness,
    MM_VolumeIncrement,
    MM_VolumeDecrement,
    MM_BassIncrement,
    MM_BassDecrement,
    MM_TrebleIncrement,
    MM_TrebleDecrement,
    MM_AL_ConsumerControl,
    MM_AL_EmailReader,
    MM_AL_Calculator,
    MM_AL_LocalMachineBrowser,
    MM_AC_Search,
    MM_AC_Home,
    MM_AC_Back,
    MM_AC_Forward,
    MM_AC_Stop,
    MM_AC_Refresh,
    MM_AC_Bookmarks,
    MM_Dashboard,
    MM_DPadUp,
    MM_DPadDown,
    MM_DPadLeft,
    MM_DPadRight,
    MM_DPadCenter,
    MM_Guide,
    MM_Live,
    MM_BugReport,
    KEY_INDEX_ENUM_GUAID
} T_KEY_INDEX_DEF;

/* Key global parameters' struct */
typedef struct
{
    uint32_t combine_keys_status;  /* to indicate the status of combined keys */
    T_KEY_HID_USAGES_BUFFER current_hid_usage_buf;  /* to indicate the current key HID usage buffer */
    T_KEY_HID_USAGES_BUFFER prev_hid_usage_buf;  /* to indicate the preivous key HID usage buffer */
    T_KEYSCAN_FIFO_DATA keyscan_fifo_data;  /* to indicate the pending keyscan FIFO data */
    T_KEY_INDEX_DEF last_long_pressed_key_index; /* to indicate the last long pressed */
} T_KEY_HANDLE_GLOBAL_DATA;

/*============================================================================*
*                        Export Global Variables
*============================================================================*/
extern T_KEY_HANDLE_GLOBAL_DATA key_handle_global_data;

/*============================================================================*
 *                         Functions
 *============================================================================*/
void key_handle_one_key_scenario(void);
bool key_handle_prepare_hid_usage_buffer(T_KEY_INDEX_DEF key_index,
                                                T_KEY_HID_USAGES_BUFFER *p_buf);
int key_handle_notify_hid_usage_buffer(T_KEY_HID_USAGES_BUFFER *p_cur_buf,
                                               T_KEY_HID_USAGES_BUFFER *p_prev_buf);
bool key_handle_notify_hid_release_event(void);
void key_handle_one_key_in_idle_status(T_KEY_INDEX_DEF key_index);
void key_handle_one_key_in_adv_status(T_KEY_INDEX_DEF key_index);
void key_handle_release_event(void);
T_KEY_INDEX_DEF key_handle_get_key_index_by_row_column(uint32_t row, uint32_t col);
T_KEY_TYPE_DEF key_handle_get_key_type_by_key_index(uint32_t key_index);
uint32_t key_handle_get_hid_usage_id_by_key_index(uint32_t key_index);
uint8_t key_handle_get_hid_usage_page_by_key_index(uint32_t key_index);
uint8_t key_handle_get_ir_key_code_by_key_index(uint32_t key_index);
void key_handle_update_kscan_fifo_data(uint32_t row, uint32_t col, bool pressed);