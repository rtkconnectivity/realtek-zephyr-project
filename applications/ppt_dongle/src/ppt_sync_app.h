/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "ppt_sync.h"
#include "board.h"

#define PPT_MOUSE_DATA_MAX_SIZE     12

#define PPT_DATA_MAX_SIZE           PPT_MOUSE_DATA_MAX_SIZE

#define PPT_DATA_MAX_SEQ_NUM        0x1f

#if FEATURE_SUPPORT_PPT_DFU
#define PPT_DFU_TX_MAX_SIZE             64 //1B 2.4G opcode + 1B seq num + 61B set_report raw data + 1B rsvd
#define PPT_DFU_COMMAND_RESPONSE_LEN    13 //ic_type + 3*image_ver
#endif

typedef enum
{
    V_WHEEL_UP      = 0x20,
    V_WHEEL_DOWN    = 0x40,
    H_WHEEL_UP      = 0x60,
    H_WHEEL_DOWN    = 0x80,
} T_WHEEL_DATA;

typedef enum
{
    X_Y_SIZE_0_BIT  = 0,
    X_Y_SIZE_2_BIT  = 1,
    X_Y_SIZE_4_BIT  = 2,
    X_Y_SIZE_6_BIT  = 3,
    X_Y_SIZE_8_BIT  = 4,
    X_Y_SIZE_10_BIT = 5,
    X_Y_SIZE_12_BIT = 6,
    X_Y_SIZE_16_BIT = 7,
} T_X_Y_SIZE;

typedef enum
{
    /* >= SYNC_OPCODE_APP_START_VALUE, <= 0x1f */
    SYNC_OPCODE_KEYBOARD        = 0x10,
    SYNC_OPCODE_CONSUMER        = 0x11,
    SYNC_OPCODE_VENDOR          = 0x12,
    SYNC_OPCODE_DPI             = 0x13,
    SYNC_OPCODE_REPORT_RATE     = 0x14,
    SYNC_OPCODE_LED_STATUS      = 0x15,
    SYNC_OPCODE_FULL_KEYBOARD   = 0x16,
    SYNC_OPCODE_PPT_FAST_PAIR   = 0x17,
    SYNC_OPCODE_PPT_DFU         = 0x18,
    SYNC_OPCODE_BAT_VALUE       = 0x19,
} T_PPT_APP_OPCODE;

typedef union
{
    /* zero and is_mouse_data_header must be 0, header_byte must <= 0x1f*/
    union
    {
        uint8_t value;
        struct
        {
            T_PPT_APP_OPCODE app_opcode: 5;
            uint8_t is_multi_data: 1;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } other_data_header;

    /* zero must be 0, is_mouse_data_header must be 1, is_multi_data must be 0 */
    union
    {
        uint8_t value;
        struct
        {
            uint8_t seq_num: 1;
            T_X_Y_SIZE x_y_bit_size: 3;
            uint8_t button_wheel_valid: 1;
            uint8_t is_multi_data: 1;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } mouse_single_data_header;

    /* zero must be 0, is_mouse_data_header must be 1, is_multi_data must be 1 */
    union
    {
        uint16_t value;
        struct
        {
            uint8_t seq_num : 5;
            T_X_Y_SIZE x2_y2_bit_size: 3;
            T_X_Y_SIZE x1_y1_bit_size: 3;
            uint8_t button2_wheel2_valid: 1;
            uint8_t button1_wheel1_valid: 1;
            uint8_t is_multi_data: 1;
            uint8_t is_mouse_data_header: 1;
            uint8_t zero: 1;
        } bit;
    } mouse_multi_data_header;
} T_PPT_SYNC_APP_HEADER;

typedef struct
{
    sync_msg_send_cb_t msg_send_cb;
    sync_bond_info_t bond_info;
    uint8_t msg_retrans_count;
    sync_bond_info_t bond_info_backup;
} T_PPT_SYNC_APP_PARA;

void sync_msg_reg_send_cb(sync_msg_send_cb_t cb);
void ppt_sync_init(sync_role_t role);
void ppt_sync_enable(void);
bool ppt_check_is_bonded(void);
bool ppt_pair(void);
bool ppt_reconnect(void);
void ppt_stop_sync(void);
bool ppt_clear_bond_info(void);
sync_err_code_t ppt_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                  uint8_t *data, uint16_t len);
