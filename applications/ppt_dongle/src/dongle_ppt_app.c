/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <string.h>
#include "dongle_ppt_app.h"
#include <zephyr/drivers/watchdog.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#if FEATURE_SUPPORT_PPT_DFU
#include "usb_dfu.h"
#endif

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
#include "dongle_ppt_trans_handle.h"
#endif
#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
#include "mp_test.h"
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#endif
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ppt_dongle);

/* default quota size for all sync message type */
#if FEATURE_SUPPORT_PROPRIETARY_HOPPING
#define SYNC_MSG_QUOTA              2
#else
#define SYNC_MSG_QUOTA              9
#endif

static uint16_t app_pm_system_suspend = 0;
static bool need_send_mouse_release_data = false;
static bool need_send_keyboard_release_data = false;
static bool need_send_full_keyboard_release_data = false;
static bool need_send_consumer_release_data = false;
struct k_timer send_release_data_after_sync_lost_timer;
struct k_timer test_slave_tx_timer;
#if (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
struct k_timer repair_after_reset_timer;
static uint16_t allow_to_repair_index = 0;
#endif
static uint8_t ppt_tx_data[65] = {0};
#if FEATURE_SUPPORT_PPT_DFU
static uint8_t ppt_dfu_tx_data[PPT_DFU_TX_MAX_SIZE] = {0};
#endif
// const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog));

T_APP_PPT_GLOBAL_DATA ppt_app_global_data;
#if FEATURE_SUPPORT_PPT_DFU
uint8_t ppt_dfu_data_seq_num = 0;
#endif

static bool ppt_change_button_wheel_to_usb_data(uint8_t *usb_buf, const uint8_t *data)
{
    usb_buf[0] = *data & 0x1f;
    uint8_t wheel_flag = *data & 0xe0;
    if (wheel_flag == 0x20)
    {
        usb_buf[5] = 0x01;
    }
    else if (wheel_flag == 0x40)
    {
        usb_buf[5] = 0xff;
    }
    else if (wheel_flag == 0x60)
    {
        usb_buf[6] = 0x01;
    }
    else if (wheel_flag == 0x80)
    {
        usb_buf[6] = 0xff;
    }
    else if (wheel_flag != 0)
    {
        return false;
    }
    return true;
}

static uint8_t ppt_change_single_x_y_to_usb_data(uint8_t *usb_buf, const uint8_t *data,
                                                 const T_X_Y_SIZE x_y_bit_size)
{
    uint8_t data_len = 0;
    if (x_y_bit_size == X_Y_SIZE_0_BIT)
    {
        return 0;
    }
    else if (x_y_bit_size == X_Y_SIZE_4_BIT)
    {
        usb_buf[1] = (data[0] >> 4) & 0x0f;
        if (data[0] & 0x80)
        {
            usb_buf[1] |= 0xf0;
            usb_buf[2] = 0xff;
        }
        usb_buf[3] = data[0] & 0x0f;
        if (data[0] & 0x08)
        {
            usb_buf[3] |= 0xf0;
            usb_buf[4] = 0xff;
        }
        data_len = 1;
    }
    else if (x_y_bit_size == X_Y_SIZE_8_BIT)
    {
        usb_buf[1] = data[0];
        if (data[0] & 0x80)
        {
            usb_buf[2] = 0xff;
        }
        usb_buf[3] = data[1];
        if (data[1] & 0x80)
        {
            usb_buf[4] = 0xff;
        }
        data_len = 2;
    }
    else if (x_y_bit_size == X_Y_SIZE_12_BIT)
    {
        usb_buf[1] = data[0];
        usb_buf[2] = (data[2] >> 4) & 0x0f;
        if (data[2] & 0x80)
        {
            usb_buf[2] |= 0xf0;
        }
        usb_buf[3] = data[1];
        usb_buf[4] = data[2] & 0x0f;
        if (data[2] & 0x08)
        {
            usb_buf[4] |= 0xf0;
        }
        data_len = 3;
    }
    else if (x_y_bit_size == X_Y_SIZE_16_BIT)
    {
        usb_buf[1] = data[0];
        usb_buf[2] = data[1];
        usb_buf[3] = data[2];
        usb_buf[4] = data[3];
        data_len = 4;
    }
    return data_len;
}

#if FEATURE_SUPPORT_MOUSE_MULTIPLE_DATA
static void ppt_change_multi_x_y_to_usb_data(uint8_t *usb_buf_1, uint8_t *usb_buf_2,
                                             const uint8_t *data,
                                             const T_PPT_SYNC_APP_HEADER ppt_header)
{
    uint8_t data_index = 0;
    T_X_Y_SIZE x_y_data_bit_size = X_Y_SIZE_0_BIT;
    uint8_t *single_usb_buf = NULL;
    if (ppt_header.mouse_multi_data_header.bit.x1_y1_bit_size == X_Y_SIZE_0_BIT)
    {
        x_y_data_bit_size = ppt_header.mouse_multi_data_header.bit.x2_y2_bit_size;
        single_usb_buf = usb_buf_2;
    }
    else if (ppt_header.mouse_multi_data_header.bit.x2_y2_bit_size == X_Y_SIZE_0_BIT)
    {
        x_y_data_bit_size = ppt_header.mouse_multi_data_header.bit.x1_y1_bit_size;
        single_usb_buf = usb_buf_1;
    }

    if (single_usb_buf != NULL)
    {
        ppt_change_single_x_y_to_usb_data(single_usb_buf, data, x_y_data_bit_size);
        return;
    }

    if (ppt_header.mouse_multi_data_header.bit.x1_y1_bit_size !=
        ppt_header.mouse_multi_data_header.bit.x2_y2_bit_size)
    {
        APP_PRINT_INFO1("[ppt_change_multi_x_y_to_usb_data] error: x1_y1_bit_size != x2_y2_bit_size, ppt_header = 0x %x",
                        ppt_header.mouse_multi_data_header.value);
        return;
    }

    x_y_data_bit_size = ppt_header.mouse_multi_data_header.bit.x1_y1_bit_size;
    if (x_y_data_bit_size == X_Y_SIZE_2_BIT)
    {
        usb_buf_1[1] = (data[0] >> 6) & 0x03;
        if (data[0] & 0x80)
        {
            usb_buf_1[1] |= 0xfc;
            usb_buf_1[2] = 0xff;
        }
        usb_buf_1[3] = (data[0] >> 4) & 0x03;
        if (data[0] & 0x20)
        {
            usb_buf_1[3] |= 0xfc;
            usb_buf_1[4] = 0xff;
        }

        usb_buf_2[1] = (data[0] >> 2) & 0x03;
        if (data[0] & 0x08)
        {
            usb_buf_2[1] |= 0xfc;
            usb_buf_2[2] = 0xff;
        }
        usb_buf_2[3] = data[0] & 0x03;
        if (data[0] & 0x02)
        {
            usb_buf_2[3] |= 0xfc;
            usb_buf_2[4] = 0xff;
        }
    }
    else if (x_y_data_bit_size == X_Y_SIZE_6_BIT)
    {
        usb_buf_1[1] = (data[0] >> 2) & 0x3f;
        if (data[0] & 0x80)
        {
            usb_buf_1[1] |= 0xc0;
            usb_buf_1[2] = 0xff;
        }
        usb_buf_1[3] = ((data[0] << 4) & 0x30) | ((data[1] >> 4) & 0x0f);
        if (data[0] & 0x02)
        {
            usb_buf_1[3] |= 0xc0;
            usb_buf_1[4] = 0xff;
        }

        usb_buf_2[1] = ((data[1] << 2) & 0x3c) | ((data[2] >> 6) & 0x03);
        if (data[1] & 0x08)
        {
            usb_buf_2[1] |= 0xc0;
            usb_buf_2[2] = 0xff;
        }
        usb_buf_2[3] = data[2] & 0x3f;
        if (data[2] & 0x20)
        {
            usb_buf_2[3] |= 0xc0;
            usb_buf_2[4] = 0xff;
        }
    }
    else if (x_y_data_bit_size == X_Y_SIZE_10_BIT)
    {
        usb_buf_1[1] = data[0];
        usb_buf_1[2] = (data[4] & 0xc0) >> 6;
        if (data[4] & 0x80)
        {
            usb_buf_1[2] |= 0xfc;
        }
        usb_buf_1[3] = data[1];
        usb_buf_1[4] = (data[4] & 0x30) >> 4;
        if (data[4] & 0x20)
        {
            usb_buf_1[4] |= 0xfc;
        }

        usb_buf_2[1] = data[2];
        usb_buf_2[2] = (data[4] & 0x0c) >> 2;
        if (data[4] & 0x08)
        {
            usb_buf_2[2] |= 0xfc;
        }
        usb_buf_2[3] = data[3];
        usb_buf_2[4] = data[4] & 0x03;
        if (data[4] & 0x02)
        {
            usb_buf_2[4] |= 0xfc;
        }
    }
    else
    {
        data_index = ppt_change_single_x_y_to_usb_data(usb_buf_1, &data[0], x_y_data_bit_size);
        ppt_change_single_x_y_to_usb_data(usb_buf_2, &data[data_index], x_y_data_bit_size);
    }
}
#endif

#if (MOUSE_PPT_APP_SEND_MODE_SEL == ADD_APP_BUFFER_FOR_PPT_MODE)
uint8_t ppt_other_data_handler(uint8_t app_opcode, uint8_t *data)
{
    uint8_t other_data_len = 0;
    if (app_opcode == SYNC_OPCODE_KEYBOARD)
    {
        other_data_len = HID_KEYBOARD_REPORT_SIZE;
        dongle_usb_hid_send_keyboard_report(&data[1], HID_KEYBOARD_REPORT_SIZE);

        need_send_keyboard_release_data = false;
        for (uint8_t i = 1; i <= HID_KEYBOARD_REPORT_SIZE; i++)
        {
            if (data[i] != 0)
            {
                need_send_keyboard_release_data = true;
                break;
            }
        }
    }
    else if (app_opcode == SYNC_OPCODE_CONSUMER)
    {
        other_data_len = HID_CONSUMER_REPORT_SIZE;
        dongle_usb_hid_send_keyboard_report(&data[1], HID_CONSUMER_REPORT_SIZE);
        need_send_consumer_release_data = false;
        for (uint8_t i = 1; i <= HID_CONSUMER_REPORT_SIZE; i++)
        {
            if (data[i] != 0)
            {
                need_send_consumer_release_data = true;
                break;
            }
        }
    }
    // else if (app_opcode == SYNC_OPCODE_VENDOR)
    // {
    //     other_data_len = CONFIG_ZMK_HID_VENDOR_REPORT_SIZE;
    //     app_usb_send_vendor_data(&data[1], USB_VENDOR_DATA_SIZE);
    // }
    else if (app_opcode == SYNC_OPCODE_DPI)
    {
        other_data_len = DPI_DATA_SIZE;
        uint8_t usb_send_data[7] = {0};
        usb_send_data[0] = data[1];
        APP_PRINT_INFO1("[ppt_other_data_handler] dpi level = %d", data[1]);
        dongle_usb_hid_send_keyboard_report(usb_send_data,7);
    }
#if FEATURE_CHANGE_USB_INTERVAL_FOR_REPORT_RATE
    else if (app_opcode == SYNC_OPCODE_REPORT_RATE)
    {
        other_data_len = REPORT_REATE_DATA_SIZE;
        uint32_t new_report_rate = data[1] | (data[2] << 8);
        APP_PRINT_INFO2("[ppt_app_receive_msg_cb] new report rate: %d, old report rate: %d",
                        new_report_rate, ppt_app_global_data.report_rate);
        if (ppt_app_global_data.report_rate != new_report_rate)
        {
            ppt_app_global_data.is_enable_to_receive_data = false;
            app_send_release_data();
            T_IO_MSG bee_io_msg = {0};
            bee_io_msg.type = IO_MSG_TYPE_REPORT_RATE_CHANGE;
            bee_io_msg.u.param = new_report_rate;
            if (false == app_send_msg_to_apptask(&bee_io_msg))
            {
                APP_PRINT_ERROR0("[ppt_app_receive_msg_cb] send IO_MSG_TYPE_REPORT_RATE_CHANGE message failed!");
            }
        }
    }
#endif

    return other_data_len;
}

static void ppt_app_receive_buffer_ctrl_msg_cb(uint8_t *data, uint16_t len,
                                               sync_receive_info_t *info)
{
    uint8_t usb_send_data1[7] = {0};
    uint8_t usb_send_data2[7] = {0};
    uint8_t data_index = 1;
    bool is_need_send_two_data = false;
    bool is_usb_buffer_2_used = false;
    T_PPT_SYNC_APP_HEADER ppt_header1 = {0};
    T_PPT_SYNC_APP_HEADER ppt_header2 = {0};
    ppt_header1.mouse_single_data_header.value = data[0];

    if (!ppt_app_global_data.is_enable_to_receive_data)
    {
        return;
    }
    if (ppt_header1.mouse_single_data_header.bit.zero != 0)
    {
        APP_PRINT_INFO0("[ppt_app_receive_buffer_ctrl_msg_cb] header.bit.zero is not 0");
        return;
    }

    /* first packet */
    if (ppt_header1.mouse_single_data_header.bit.is_mouse_data_header == 1)
    {
        /* button and wheel data */
        if (ppt_header1.mouse_single_data_header.bit.button_wheel_valid == 1)
        {
            if (ppt_change_button_wheel_to_usb_data(usb_send_data2, &data[data_index]))
            {
                data_index++;
            }
            else
            {
                APP_PRINT_INFO0("[ppt_app_receive_buffer_ctrl_msg_cb] wheel data is error");
                return;
            }
        }
        /* x y data */
        T_X_Y_SIZE x_y_data_bit_size = ppt_header1.mouse_single_data_header.bit.x_y_bit_size;
        uint8_t temp_data_index = ppt_change_single_x_y_to_usb_data(usb_send_data2, &data[data_index],
                                                                    x_y_data_bit_size);
        data_index += temp_data_index;
        is_usb_buffer_2_used = true;
    }
    else
    {
        uint8_t other_data_len = ppt_other_data_handler(ppt_header1.other_data_header.bit.app_opcode, data);
        data_index += other_data_len;
    }

    if (ppt_header1.mouse_single_data_header.bit.is_multi_data == 1)
    {
        ppt_header2.mouse_single_data_header.value = data[data_index++];
        /* second packet */
        if ((ppt_header2.mouse_single_data_header.bit.is_mouse_data_header == 1) &&
            (is_usb_buffer_2_used == false))
        {
            /* button and wheel data */
            if (ppt_header2.mouse_single_data_header.bit.button_wheel_valid == 1)
            {
                if (ppt_change_button_wheel_to_usb_data(usb_send_data2, &data[data_index]))
                {
                    data_index++;
                }
                else
                {
                    APP_PRINT_INFO0("[ppt_app_receive_buffer_ctrl_msg_cb] wheel data is error");
                    return;
                }
            }
            /* x y data */
            T_X_Y_SIZE x_y_data_bit_size = ppt_header2.mouse_single_data_header.bit.x_y_bit_size;
            uint8_t temp_data_index = ppt_change_single_x_y_to_usb_data(usb_send_data2, &data[data_index],
                                                                        x_y_data_bit_size);
            data_index += temp_data_index;
            is_usb_buffer_2_used = true;
        }
        else if ((ppt_header2.mouse_single_data_header.bit.is_mouse_data_header == 1) &&
                 (is_usb_buffer_2_used == true))
        {
            /* button2 and wheel2 data */
            if (ppt_header2.mouse_single_data_header.bit.button_wheel_valid == 1)
            {
                if (ppt_change_button_wheel_to_usb_data(usb_send_data1, &data[data_index]))
                {
                    data_index++;
                }
                else
                {
                    APP_PRINT_INFO0("[ppt_app_receive_buffer_ctrl_msg_cb] wheel data is error");
                    return;
                }
            }
            /* x y data */
            T_X_Y_SIZE x2_y2_data_bit_size = ppt_header2.mouse_single_data_header.bit.x_y_bit_size;
            uint8_t temp_data_index = ppt_change_single_x_y_to_usb_data(usb_send_data1, &data[data_index],
                                                                        x2_y2_data_bit_size);
            data_index += temp_data_index;
            is_need_send_two_data = true;
        }
        else
        {
            uint8_t other_data_len_2 = ppt_other_data_handler(ppt_header2.other_data_header.bit.app_opcode,
                                                              data);
            data_index += other_data_len_2;
        }
    }

    /* send data to usb */
    if (is_need_send_two_data == true)
    {
        dongle_usb_hid_send_keyboard_report(usb_send_data2,7);
        dongle_usb_hid_send_keyboard_report(usb_send_data1,7);
        if (usb_send_data1[0] == 0)
        {
            need_send_mouse_release_data = false;
        }
        else
        {
            need_send_mouse_release_data = true;
        }
    }
    else
    {
        dongle_usb_hid_send_keyboard_report(usb_send_data2,7);
        if (usb_send_data2[0] == 0)
        {
            need_send_mouse_release_data = false;
        }
        else
        {
            need_send_mouse_release_data = true;
        }
    }
}
#endif

static void ppt_app_receive_msg_cb(uint8_t *data, uint16_t len, sync_receive_info_t *info)
{
//#if !IS_RELEASE_VERSION
//    APP_PRINT_INFO2("[ppt_app_receive_msg_cb] len = %d, data = 0x %b", len, TRACE_BINARY(len, data));
//#endif
#if (MOUSE_PPT_APP_SEND_MODE_SEL == ADD_APP_BUFFER_FOR_PPT_MODE)
    ppt_app_receive_buffer_ctrl_msg_cb(data, len, info);
#endif
#if (MOUSE_PPT_APP_SEND_MODE_SEL == NORMAL_SINGLE_DATA_MODE)
    uint8_t usb_send_data1[7] = {0};
    uint8_t usb_send_data2[7] = {0};
    uint8_t data_index = 1;
    bool is_need_send_two_data = false;
    T_PPT_SYNC_APP_HEADER ppt_header = {0};
    ppt_header.mouse_single_data_header.value = data[0];

    if (!ppt_app_global_data.is_enable_to_receive_data)
    {
        return;
    }
    if (ppt_header.mouse_single_data_header.bit.zero != 0)
    {
        APP_PRINT_INFO0("[ppt_app_receive_msg_cb] header.bit.zero is not 0");
        return;
    }

    if (ppt_header.mouse_single_data_header.bit.is_mouse_data_header == 1)
    {
        if (ppt_header.mouse_single_data_header.bit.is_multi_data == 0)
        {
            /* single data */
            /* button and wheel data */
            if (ppt_header.mouse_single_data_header.bit.button_wheel_valid == 1)
            {
                if (ppt_change_button_wheel_to_usb_data(usb_send_data2, &data[data_index]))
                {
                    data_index++;
                }
                else
                {
                    APP_PRINT_INFO0("[ppt_app_receive_msg_cb] wheel data is error");
                    return;
                }
            }
            /* x y data */
            T_X_Y_SIZE x_y_data_bit_size = ppt_header.mouse_single_data_header.bit.x_y_bit_size;
            ppt_change_single_x_y_to_usb_data(usb_send_data2, &data[data_index], x_y_data_bit_size);
            ppt_app_global_data.mouse_data_current_seq_num = ppt_header.mouse_single_data_header.bit.seq_num;
        }
#if FEATURE_SUPPORT_MOUSE_MULTIPLE_DATA
        else
        {
            /* multi data */
            ppt_header.mouse_multi_data_header.value = (data[0] << 8) | data[1];
            data_index = 2;
            if ((ppt_app_global_data.mouse_data_current_seq_num == PPT_DATA_MAX_SEQ_NUM &&
                 ppt_header.mouse_multi_data_header.bit.seq_num == 2) ||
                (ppt_app_global_data.mouse_data_current_seq_num + 1 ==
                 ppt_header.mouse_multi_data_header.bit.seq_num))
            {
                is_need_send_two_data = false;
            }
            else
            {
                is_need_send_two_data = true;
            }
            /* button and wheel data */
            if (ppt_header.mouse_multi_data_header.bit.button1_wheel1_valid == 1)
            {
                if (is_need_send_two_data == true)
                {
                    if (ppt_change_button_wheel_to_usb_data(usb_send_data1, &data[data_index]) == false)
                    {
                        APP_PRINT_INFO0("[ppt_app_receive_msg_cb] wheel data is error");
                        return;
                    }
                }
                data_index++;
            }
            if (ppt_header.mouse_multi_data_header.bit.button2_wheel2_valid == 1)
            {
                if (ppt_change_button_wheel_to_usb_data(usb_send_data2, &data[data_index]))
                {
                    data_index++;
                }
                else
                {
                    APP_PRINT_INFO0("[ppt_app_receive_msg_cb] wheel data is error");
                    return;
                }
            }
            /* x y data */
            ppt_change_multi_x_y_to_usb_data(usb_send_data1, usb_send_data2, &data[data_index], ppt_header);
            ppt_app_global_data.mouse_data_current_seq_num = ppt_header.mouse_multi_data_header.bit.seq_num;
        }
#endif
        /* send data to usb */
        if (is_need_send_two_data == true)
        {
            dongle_usb_hid_send_keyboard_report(usb_send_data1,7);
            dongle_usb_hid_send_keyboard_report(usb_send_data2,7);
        }
        else
        {
            dongle_usb_hid_send_keyboard_report(usb_send_data2,7);
        }
        if (usb_send_data2[0] == 0)
        {
            need_send_mouse_release_data = false;
        }
        else
        {
            need_send_mouse_release_data = true;
        }
    }
    else
    {
        if (dongle_ppt_trans_rx_raw_cb(data, len, info) == true)
        {
            return;
        }

        ppt_header.other_data_header.value = data[0];
        if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_KEYBOARD)
        {
            if (len == HID_KEYBOARD_REPORT_SIZE + 1)
            {
                dongle_usb_hid_send_keyboard_report(&data[1], HID_KEYBOARD_REPORT_SIZE);
            }
            need_send_keyboard_release_data = false;
            for (uint8_t i = 1; i <= HID_KEYBOARD_REPORT_SIZE; i++)
            {
                if (data[i] != 0)
                {
                    need_send_keyboard_release_data = true;
                    break;
                }
            }
        }
        else if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_FULL_KEYBOARD)
        {
            // if (len == USB_FULL_KEYBOARD_DATA_SIZE + 1)
            // {
            //     dongle_usb_hid_send_keyboard_report(&data[1], USB_FULL_KEYBOARD_DATA_SIZE);
            // }
            // need_send_full_keyboard_release_data = false;
            // for (uint8_t i = 1; i <= USB_FULL_KEYBOARD_DATA_SIZE; i++)
            // {
            //     if (data[i] != 0)
            //     {
            //         need_send_full_keyboard_release_data = true;
            //         break;
            //     }
            // }
        }
        else if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_CONSUMER)
        {
            if (len == HID_CONSUMER_REPORT_SIZE + 1)
            {
                dongle_usb_hid_send_keyboard_report(&data[1], HID_CONSUMER_REPORT_SIZE);
            }
            need_send_consumer_release_data = false;
            for (uint8_t i = 1; i <= HID_CONSUMER_REPORT_SIZE; i++)
            {
                if (data[i] != 0)
                {
                    need_send_consumer_release_data = true;
                    break;
                }
            }
        }
        else if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_VENDOR)
        {
            // if (len == USB_VENDOR_DATA_SIZE + 1)
            // {
            //     dongle_usb_hid_send_keyboard_report(&data[1], USB_VENDOR_DATA_SIZE);
            // }
        }
        else if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_DPI)
        {
            if (len == DPI_DATA_SIZE + 1)
            {
                usb_send_data1[0] = data[1];
                dongle_usb_hid_send_keyboard_report(usb_send_data1,7);
            }
        }
#if FEATURE_CHANGE_USB_INTERVAL_FOR_REPORT_RATE
        else if (ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_REPORT_RATE)
        {
            if (len == REPORT_REATE_DATA_SIZE + 1)
            {
                uint32_t new_report_rate = data[1] | (data[2] << 8);
                APP_PRINT_INFO2("[ppt_app_receive_msg_cb] new report rate: %d, old report rate: %d",
                                new_report_rate, ppt_app_global_data.report_rate);
                if (ppt_app_global_data.report_rate != new_report_rate)
                {
                    ppt_app_global_data.is_enable_to_receive_data = false;
                    app_send_release_data();
                    T_IO_MSG bee_io_msg = {0};
                    bee_io_msg.type = IO_MSG_TYPE_REPORT_RATE_CHANGE;
                    bee_io_msg.u.param = new_report_rate;
                    if (false == app_send_msg_to_apptask(&bee_io_msg))
                    {
                        APP_PRINT_ERROR0("[ppt_app_receive_msg_cb] send IO_MSG_TYPE_REPORT_RATE_CHANGE message failed!");
                    }
                }
            }
        }
#endif
#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
        else if ((ppt_header.other_data_header.bit.app_opcode == SYNC_OPCODE_PPT_FAST_PAIR))
        {
            if (len == 2 && data[1] == 0xA5)
            {
                k_timer_stop(&ppt_fast_pair_status_notify_timer);
            }
        }
#endif
    }
#endif
}

static void ppt_app_send_msg_cb(sync_msg_type_t type, uint8_t *data, uint16_t len,
                                sync_send_info_t *info)
{
    APP_PRINT_INFO3("ppt_app_send_msg_cb, type: %d, len: %d, send_result: %d!", type, len, info->res);
}

static void ppt_app_sync_event_cb(sync_event_t event)
{
    switch (event)
    {
    case SYNC_EVENT_PAIRED:
        {
            APP_PRINT_INFO0("[ppt_app_sync_event_cb] SYNC_EVENT_PAIRED");
            ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_PAIRED;
            ppt_app_global_data.is_ppt_bond = true;
        }
        break;
    case SYNC_EVENT_PAIR_TIMEOUT:
        {
            APP_PRINT_INFO0("[ppt_app_sync_event_cb] SYNC_EVENT_PAIR_TIMEOUT");
            ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_IDLE;
            dongle_ppt_pair();
        }
        break;
    case SYNC_EVENT_CONNECTED:
        {
            APP_PRINT_INFO0("[ppt_app_sync_event_cb] SYNC_EVENT_CONNECTED");
            ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_CONNECTED;
            k_timer_stop(&send_release_data_after_sync_lost_timer);
#if (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
            k_timer_stop(&repair_after_reset_timer);
            allow_to_repair_index = 0;
#endif

            bool ret;
            uint32_t ppt_interval = 125;
            ret = sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
            if (ret == false)
            {
                APP_PRINT_ERROR0("[ppt_app_sync_event_cb] sync_time_get failed");
            }
            else
            {
                uint32_t report_rate = 1000000 / ppt_interval;
                APP_PRINT_INFO2("[ppt_app_sync_event_cb] new report rate: %d, old report rate: %d",
                                report_rate, ppt_app_global_data.report_rate);

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
#if !FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
                if (report_rate != PPT_REPORT_RATE_LEVEL_8K)
                {
                    sync_msg_reg_receive_cb(ppt_app_receive_msg_cb);
                    ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT, false);
                }
                else
                {
                    ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT, true);
                }
#endif
#if (FEATURE_PROPRIETARY_TRANSPORT_USE_16BTIS_UNDER_1K == 1 \
    && PPT_TRANS_FEATURE_SUPPORT_16BITS_MOV)
                if (report_rate < PPT_REPORT_RATE_LEVEL_1K)
                {
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
                    if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV) == false)
                    {
                        ppt_trans_pos_ctrl_init();
                        ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler(PPT_TRANS_APPLICATION_LINK_LOST,
                                                                           NULL, NULL);
                    }
#endif
                    ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV, true);
                }
                else
                {
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
                    if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV) == true)
                    {
                        ppt_trans_pos_ctrl_init();
                        ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler(PPT_TRANS_APPLICATION_LINK_LOST,
                                                                           NULL, NULL);
                    }
#endif
                    ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV, false);
                }
#endif // FEATURE_PROPRIETARY_TRANSPORT_USE_16BTIS_UNDER_1K
#endif // FEATURE_SUPPORT_PROPRIETARY_TRANSPORT

#if FEATURE_SUPPORT_PPT_DFU
                if (!remote_dfu_info_achieved && (ppt_app_global_data.is_enable_to_receive_data == true))
                {
                    uint8_t temp_ppt_dfu_request[] = {PPT_DFU_GET_REMOTE_VER_CMD};
                    dongle_app_send_ppt_dfu(temp_ppt_dfu_request, sizeof(temp_ppt_dfu_request), false);
                }
#endif
#if FEATURE_CHANGE_USB_INTERVAL_FOR_REPORT_RATE
                if (report_rate != ppt_app_global_data.report_rate)
                {
                    ppt_app_global_data.is_enable_to_receive_data = false;
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
                    if (ppt_app_global_data.report_rate == PPT_REPORT_RATE_LEVEL_8K)
                    {
                        dongle_ppt_trans_handle_set_check_release();
                    }
                    else
#endif // FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
                    {
                        app_send_release_data();
                    }
                    T_IO_MSG bee_io_msg = {0};
                    bee_io_msg.type = IO_MSG_TYPE_REPORT_RATE_CHANGE;
                    bee_io_msg.u.param = report_rate;
                    ppt_app_global_data.report_rate = report_rate;
                    if (false == app_send_msg_to_apptask(&bee_io_msg))
                    {
                        APP_PRINT_ERROR0("send IO_MSG_TYPE_REPORT_RATE_CHANGE message failed!");
                    }
                }
#endif // FEATURE_CHANGE_USB_INTERVAL_FOR_REPORT_RATE
            }
#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
            if (ppt_app_global_data.is_ppt_fast_pair_mode)
            {
                dongle_app_send_ppt_fast_pair_status();
                k_timer_start(&ppt_fast_pair_status_notify_timer, K_MSEC(PPT_FAST_PAIR_STATUS_NOTIFY_TIME), K_NO_WAIR);
            }
#endif
        }
        k_timer_start(&test_slave_tx_timer, K_MSEC(1000), K_MSEC(1000));
        break;
    case SYNC_EVENT_CONNECT_TIMEOUT:
        {
            APP_PRINT_INFO0("[ppt_app_sync_event_cb] SYNC_EVENT_CONNECT_TIMEOUT");
            ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_IDLE;
            if (ppt_app_global_data.is_ppt_bond)
            {
                dongle_ppt_reconnect();
            }
        }
        break;
    case SYNC_EVENT_CONNECT_LOST:
        {
            APP_PRINT_INFO0("[ppt_app_sync_event_cb] SYNC_EVENT_CONNECT_LOST");
            ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_IDLE;

            if (ppt_app_global_data.is_ppt_bond)
            {
                dongle_ppt_reconnect();
            }
            else
            {
                dongle_ppt_pair();
            }

            k_timer_start(&send_release_data_after_sync_lost_timer,
                            K_MSEC(SEND_RELEASE_DATA_TIMEOUT), K_NO_WAIT);
        }
        break;
    default:
        break;
    }

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
    dongle_ppt_trans_handle_sync_evt_hook(event);
#endif
}

static void app_send_release_data(void)
{
    ppt_app_global_data.mouse_data_current_seq_num = 0;

    uint8_t key_release_data[15] = {0};
    if (need_send_mouse_release_data)
    {
        APP_PRINT_INFO0("send mouse release data!");
        dongle_usb_hid_send_keyboard_report(key_release_data,15);
        need_send_mouse_release_data = false;
    }
    if (need_send_keyboard_release_data)
    {
        APP_PRINT_INFO0("send keyboard release data!");
        dongle_usb_hid_send_keyboard_report(key_release_data, HID_KEYBOARD_REPORT_SIZE);
        need_send_keyboard_release_data = false;
    }
    if (need_send_full_keyboard_release_data)
    {
        APP_PRINT_INFO0("send full keyboard release data!");
        // dongle_usb_hid_send_keyboard_report(key_release_data, USB_FULL_KEYBOARD_DATA_SIZE);
        need_send_full_keyboard_release_data = false;
    }
    if (need_send_consumer_release_data)
    {
        APP_PRINT_INFO0("send consumer release data!");
        dongle_usb_hid_send_keyboard_report(key_release_data, HID_CONSUMER_REPORT_SIZE);
        need_send_consumer_release_data = false;
    }
}

static void send_release_data_after_sync_lost_timer_callback(struct k_timer *p_timer)
{
    APP_PRINT_INFO0("send_release_data_after_sync_lost_timer_callback timeout!");
    app_send_release_data();
#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
    if (ppt_app_global_data.is_ppt_fast_pair_mode)
    {
        if (ppt_app_global_data.dongle_ppt_status != DONGLE_PPT_STATUS_IDLE)
        {
            dongle_ppt_stop_sync();
        }
        if (ppt_clear_bond_info())
        {
            dongle_ppt_pair();
        }
    }
#endif
}

static void send_slave_data_periodly(struct k_timer *p_timer)
{
    APP_PRINT_INFO0("test send_slave_data_periodly timeout!");
    uint8_t data[] =
    {
        0x01, 0x02, 0x01, 0x02, 0x1, 0x1, //!< AdvA
        /* advertising data */
        0x2, 0x1, 0x5, //!< flags
        0x5, 0x9, '2', '.', '4', 'g' //!< complete local name
    };
    uint8_t len = sizeof(data);
    ppt_push_tx_data(len, data);
}

#if (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
static void repair_after_reset_timer_callback(struct k_timer *p_timer)
{
    allow_to_repair_index ++;
    if (allow_to_repair_index >= ALLOW_TO_REPAIR_TIME)
    {
        allow_to_repair_index = 0;
        if (ppt_app_global_data.dongle_ppt_status == DONGLE_PPT_STATUS_PAIRING)
        {
            dongle_ppt_stop_sync();
            dongle_ppt_reconnect();
        }
    }
    else
    {
        k_timer_start(&repair_after_reset_timer,K_MSEC(REPAIR_OR_RECONNECT_PERIOD),K_NO_WAIT);
        if (ppt_app_global_data.dongle_ppt_status == DONGLE_PPT_STATUS_CONNECTING)
        {
            dongle_ppt_stop_sync();
            dongle_ppt_pair();
        }
        else if (ppt_app_global_data.dongle_ppt_status == DONGLE_PPT_STATUS_PAIRING)
        {
            dongle_ppt_stop_sync();
            dongle_ppt_reconnect();
        }
    }
}
#endif

void pm_check_status_before_enter_wfi_or_dlps(void)
{
#if DLPS_EN
    if (app_pm_system_suspend > 0)
    {
        power_mode_resume();
        app_pm_system_suspend--;
    }
    APP_PRINT_INFO1("[pm_check_status_before_] app_pm_system_suspend = %d", app_pm_system_suspend);
#endif
}

void pm_no_check_status_before_enter_wfi(void)
{
    power_mode_pause();
    app_pm_system_suspend++;
    APP_PRINT_INFO1("[pm_no_check_status_before_] app_pm_system_suspend = %d", app_pm_system_suspend);
}

#if (WATCH_DOG_ENABLE == 1)
void app_watchdog_open(uint32_t ms, WDTMode_TypeDef wdt_mode)
{
    if (!ppt_app_global_data.is_aon_wdg_enable)
    {
        WDT_Start(ms, wdt_mode);
        ppt_app_global_data.is_aon_wdg_enable = true;
        os_timer_restart(&watch_dog_reset_dlps_timer, WATCH_DOG_TIMEOUT_MS - 1000);
    }
}

void app_watchdog_close(void)
{
    if (ppt_app_global_data.is_aon_wdg_enable)
    {
        WDT_Disable();
        ppt_app_global_data.is_aon_wdg_enable = false;
        os_timer_stop(&watch_dog_reset_dlps_timer);
    }
}
#endif

void app_system_reset(uint8_t reset_reason)
{
#if (WATCH_DOG_ENABLE == 1)
    app_watchdog_close();
#endif
    DBG_DIRECT("app_system_reset");
    // struct wdt_timeout_cfg wdt_config = {
    //     .flags = reset_reason,
    //     .window.max = 0, // set window.max to 0: reboot immediately
    // };
    // wdt_install_timeout(wdt, &wdt_config);
}

void dongle_ppt_pair(void)
{
    if (true == ppt_pair())
    {
        APP_PRINT_INFO0("ppt start pair success");
        ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_PAIRING;
        ppt_app_global_data.mouse_data_current_seq_num = 0;
    }
    else
    {
        APP_PRINT_ERROR0("ppt start pair fail!");
    }
}

void dongle_ppt_reconnect(void)
{
    if (true == ppt_reconnect())
    {
        ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_CONNECTING;
    }
}

void dongle_ppt_stop_sync(void)
{
    APP_PRINT_INFO0("dongle_ppt_stop_sync");
    ppt_stop_sync();
    ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_IDLE;
    ppt_app_global_data.mouse_data_current_seq_num = 0;
}

void dongle_ppt_init(void)
{
    APP_PRINT_INFO0("dongle_ppt_init");
    ppt_sync_init(SYNC_ROLE_SLAVE);

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
    dongle_ppt_trans_handle_init();
#if !FEATURE_SUPPORT_PROPRIETARY_HOPPING
    /*set crc 16*/
    sync_crc_set(16, 0x8005, 0xffff);
#endif
#else
    sync_msg_reg_receive_cb(ppt_app_receive_msg_cb);
    /*set crc 16*/
    sync_crc_set(16, 0x8005, 0xffff);
#endif

#if !FEATURE_SUPPORT_PROPRIETARY_HOPPING
    /*config channel map*/
    uint16_t chans[] = {PPT_TRANS_SYNC_CHANS};
    sync_channel_set(sizeof(chans) / sizeof(uint16_t), 3, chans);
#endif

// #ifdef PPT_PAIR_ACC
//     sync_acc_t pair_acc = {.addr = PPT_PAIR_ACC};
//     sync_acc_set_br(pair_acc);
// #endif

    sync_msg_reg_send_cb(ppt_app_send_msg_cb);
    sync_event_cb_reg(ppt_app_sync_event_cb);

    ppt_app_global_data.is_ppt_bond = ppt_check_is_bonded();

    sync_pair_rssi_set(PPT_PAIR_RSSI_THRESHOLD);

    uint8_t msg_quota[SYNC_MSG_TYPE_NUM] = {0, SYNC_MSG_QUOTA, SYNC_MSG_QUOTA, SYNC_MSG_QUOTA};
    sync_msg_set_quota(msg_quota);
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_set_pos_pos_seq_gap(SYNC_MSG_QUOTA);
#endif
    k_timer_init(&send_release_data_after_sync_lost_timer, send_release_data_after_sync_lost_timer_callback,NULL);
    k_timer_init(&test_slave_tx_timer, send_slave_data_periodly,NULL);

#if ENABLE_2_4G_LOG
    sync_log_set(0, true);
#else
    sync_log_set(0, false);
#endif
}

void dongle_ppt_enable(void)
{
    APP_PRINT_INFO0("dongle_ppt_enable");
    ppt_sync_enable();
    ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_IDLE;
#if FAST_PAIR_TEST_MODE
    if (ppt_app_global_data.is_ppt_fast_pair_mode == true)
    {
        dongle_ppt_pair();
        return;
    }
#endif
#if (DONGLE_REPAIR_MODE == NOT_ALLOW_TO_REPAIR)
    if (ppt_app_global_data.is_ppt_bond)
    {
        dongle_ppt_reconnect();
    }
    else
    {
        dongle_ppt_pair();
    }
#elif (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
    if (ppt_app_global_data.is_ppt_bond)
    {
        APP_PRINT_INFO0("no bond info, init and start repair_after_reset_timer");
        k_timer_init(&repair_after_reset_timer, repair_after_reset_timer_callback, NULL);
        k_timer_start(&repair_after_reset_timer, K_MSEC(REPAIR_OR_RECONNECT_PERIOD),K_NO_WAIT);
        dongle_ppt_reconnect();
    }
    else
    {
        APP_PRINT_INFO0("dongle ppt pair");
        dongle_ppt_pair();
    }
#endif
}

void dongle_app_send_led_status(uint8_t led_status)
{
    if (ppt_app_global_data.dongle_ppt_status == DONGLE_PPT_STATUS_CONNECTED)
    {
        uint8_t ppt_tx_data[2] = {0};
        ppt_tx_data[0] = SYNC_OPCODE_LED_STATUS;
        ppt_tx_data[1] = led_status;
        ppt_app_send_data(SYNC_MSG_TYPE_INFINITE_RETRANS, 0, ppt_tx_data, 2);
    }
}

#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
void dongle_app_send_ppt_fast_pair_status(void)
{
    if (ppt_app_global_data.dongle_ppt_status == DONGLE_PPT_STATUS_CONNECTED)
    {
        uint8_t ppt_tx_data[2] = {0};
        ppt_tx_data[0] = SYNC_OPCODE_PPT_FAST_PAIR;
        ppt_tx_data[1] = 0xA5; //0xA5 means in fast pair mode
        ppt_app_send_data(SYNC_MSG_TYPE_INFINITE_RETRANS, 0, ppt_tx_data, 2);
    }
}
#endif

void dongle_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count, uint8_t *data,
                          uint16_t len)
{
    ppt_app_send_data(type, msg_retrans_count, *data, len);
}

void app_init_ppt_global_data(void)
{
    memset(&ppt_app_global_data, 0, sizeof(ppt_app_global_data));
    ppt_app_global_data.dongle_ppt_status = DONGLE_PPT_STATUS_DEFAULT;
#if FEATURE_CHANGE_USB_INTERVAL_FOR_REPORT_RATE
    if (0 != ftl_load_from_module("app", &ppt_app_global_data.report_rate, FTL_REPORT_RATE_OFFSET,
                                  FTL_REPORT_RATE_LEN))
    {
        ppt_app_global_data.report_rate = PPT_DEFAULT_REPORT_RATE;
    }
#endif
}

void dongle_app_send_get_bat_value(void)
{
    T_PPT_SYNC_APP_HEADER header = {.other_data_header = {.bit.app_opcode = SYNC_OPCODE_BAT_VALUE}};
    ppt_tx_data[0] = header.other_data_header.value;
    sync_err_code_t ret = ppt_app_send_data(SYNC_MSG_TYPE_INFINITE_RETRANS, 0, ppt_tx_data, 1);
    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_INFO0("[dongle_app_send_get_bat_value] send fail");
    }
}

#if FEATURE_SUPPORT_PPT_DFU
void dongle_app_send_ppt_dfu(uint8_t *ppt_dfu_data, uint8_t dfu_data_len, bool need_ppt_dfu_seq)
{
    sync_err_code_t ret = SYNC_ERR_CODE_NOT_FOUND;
    T_PPT_SYNC_APP_HEADER header = {.other_data_header = {.bit.app_opcode = SYNC_OPCODE_PPT_DFU}};
    ppt_dfu_tx_data[0] = header.other_data_header.value;

    if (need_ppt_dfu_seq == true)
    {
        ppt_dfu_tx_data[1] = ppt_dfu_data_seq_num;
        memcpy(&ppt_dfu_tx_data[2], ppt_dfu_data, dfu_data_len);
        ret = ppt_app_send_data(SYNC_MSG_TYPE_INFINITE_RETRANS, 0, ppt_dfu_tx_data, dfu_data_len + 2);

        if (ppt_dfu_tx_data[2] == REPORT_ID_PPT_DFU_OFFER_REQUEST)
        {
            is_ppt_dfu_flag = true;
        }
        if (!ret)
        {
            ppt_dfu_data_seq_num ++;
        }
    }
    else
    {
        memcpy(&ppt_dfu_tx_data[1], ppt_dfu_data, dfu_data_len);
        ret = ppt_app_send_data(SYNC_MSG_TYPE_INFINITE_RETRANS, 0, ppt_dfu_tx_data, dfu_data_len + 1);
    }

    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_INFO1("[app_ppt_send_dfu_data] send fail, error reason = %d", ret);
    }
}

static void dongle_ppt_dfu_rx_raw_cb(uint8_t *p_data, uint16_t len)
{
    if (len == PPT_DFU_COMMAND_RESPONSE_LEN) //GETVER's response
    {
        remote_dfu_device_ic_type = p_data[0];
        memcpy(&remote_dfu_device_patch_ver, &p_data[1], sizeof(remote_dfu_device_patch_ver));
        memcpy(&remote_dfu_device_upperstack_ver, &p_data[5], sizeof(remote_dfu_device_upperstack_ver));
        memcpy(&remote_dfu_device_app_ver, &p_data[9], sizeof(remote_dfu_device_app_ver));

        remote_dfu_info_achieved = true;

        APP_PRINT_INFO1("[dongle_ppt_dfu_rx_raw_cb] remote dfu device ic type = 0x%x",
                        remote_dfu_device_ic_type);
        APP_PRINT_INFO4("[dongle_ppt_dfu_rx_raw_cb] remote dfu device MCUPATCH version = %d.%d.%d.%d",
                        remote_dfu_device_patch_ver.ver_info.img_sub_version._version_major,
                        remote_dfu_device_patch_ver.ver_info.img_sub_version._version_minor,
                        remote_dfu_device_patch_ver.ver_info.img_sub_version._version_revision,
                        remote_dfu_device_patch_ver.ver_info.img_sub_version._version_reserve);
        APP_PRINT_INFO4("[dongle_ppt_dfu_rx_raw_cb] remote dfu device UPPERSTACK version = %d.%d.%d.%d",
                        remote_dfu_device_upperstack_ver.ver_info.img_sub_version._version_major,
                        remote_dfu_device_upperstack_ver.ver_info.img_sub_version._version_minor,
                        remote_dfu_device_upperstack_ver.ver_info.img_sub_version._version_revision,
                        remote_dfu_device_upperstack_ver.ver_info.img_sub_version._version_reserve);
        APP_PRINT_INFO4("[dongle_ppt_dfu_rx_raw_cb] remote dfu device MCUAPP version = %d.%d.%d.%d",
                        remote_dfu_device_app_ver.ver_info.img_sub_version._version_major,
                        remote_dfu_device_app_ver.ver_info.img_sub_version._version_minor,
                        remote_dfu_device_app_ver.ver_info.img_sub_version._version_revision,
                        remote_dfu_device_app_ver.ver_info.img_sub_version._version_reserve);
    }
    else if (len == (PPT_DFU_OFFER_RESPONSE_LEN + 1) || (len == (PPT_DFU_DATA_RESPONSE_LEN + 1)))
    {
        //handle payload bin response from mouse
        app_usb_send_dfu_data(p_data[0], &p_data[1], len - 1);
        if (len == (PPT_DFU_OFFER_RESPONSE_LEN + 1))
        {
            T_CFU_OFFER_RESPONSE cfu_offer_response = {0};
            memcpy((uint8_t *)&cfu_offer_response, &p_data[1], PPT_DFU_OFFER_RESPONSE_LEN);
            app_dfu_error_status_check(cfu_offer_response.status);
        }
        else if (len == (PPT_DFU_DATA_RESPONSE_LEN + 1))
        {
            T_CFU_DATA_RESPONSE cfu_data_response = {0};
            memcpy((uint8_t *)&cfu_data_response, &p_data[1], PPT_DFU_DATA_RESPONSE_LEN);
            app_dfu_error_status_check(cfu_data_response.status);
        }
    }
}
#endif

bool dongle_ppt_trans_rx_raw_cb(uint8_t *p_data, uint16_t len, sync_receive_info_t *info)
{
    if (p_data[0] == SYNC_OPCODE_BAT_VALUE)
    {
        uint8_t usb_data[6] = {0};
        usb_data[0] = 0x62;
        usb_data[1] = 0x61;
        usb_data[2] = 0x74;
        usb_data[3] = p_data[1];
        usb_data[4] = p_data[2];
        usb_data[5] = p_data[3];
        // app_usb_send_dfu_data(REPORT_ID_MP_CMD, usb_data, 6);
    }
#if FEATURE_SUPPORT_PPT_DFU
    else if (p_data[0] == SYNC_OPCODE_PPT_DFU)
    {
        dongle_ppt_dfu_rx_raw_cb(&p_data[1], len - 1);
    }
#endif
    else
    {
        return false;
    }
    return true;
}
