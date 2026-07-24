/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdint.h"
#include "string.h"
#include "board.h"
#include "trace.h"
#include "dongle_ppt_app.h"
#include "ppt_trans_handle.h"
#include "dongle_ppt_trans_handle.h"
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
#include "ppt_trans_chann_ctrl.h"
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#endif

#if DONGLE_PPT_TRANS_PRINT_STATS_EN
#include "os_timer.h"
#endif

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT

uint8_t dongle_usb_pkt_table[DONGLE_PPT_TRANS_HIST_PKT_NUM][USB_MOUSE_DATA_LEN + 1] = {0};
uint32_t dongle_pos_table[DONGLE_PPT_TRANS_HIST_PKT_NUM][2] = {0};
static uint8_t dongle_usb_pkt_table_read_idx = 0;
static uint8_t dongle_usb_pkt_table_write_idx = 0;
static uint8_t dongle_usb_pkt_button_mask = 0;
static bool    dongle_usb_need_check_release = false;
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
static void *dongle_ppt_trans_dump_stats_timer = NULL;
static uint32_t dongle_ppt_trans_usb_send_cnt = 0;
static uint16_t dongle_ppt_trans_miss_cnt[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
#endif

static volatile int32_t dongle_ppt_trans_sync_buffer_x = 0;
static volatile int32_t dongle_ppt_trans_sync_buffer_y = 0;

static inline void dongle_ppt_update_read_idx(uint8_t next_idx)
{
    uint32_t s = os_lock();
    dongle_usb_pkt_table_read_idx = (next_idx) % DONGLE_PPT_TRANS_HIST_PKT_NUM;
    os_unlock(s);
}

static inline void dongle_ppt_update_write_idx(uint8_t next_idx)
{
    uint32_t s = os_lock();
    dongle_usb_pkt_table_write_idx = (next_idx) % DONGLE_PPT_TRANS_HIST_PKT_NUM;
    os_unlock(s);
}

static inline void dongle_ppt_send_release_button_pkt(void)
{
    uint8_t release_data[USB_MOUSE_DATA_LEN] = {0};
    if (dongle_usb_pkt_button_mask != 0)
    {
        if (true == app_usb_send_mouse_data(release_data))
        {
            APP_PRINT_INFO1("[dongle_ppt_send_release_button_pkt] prev button mask: 0x%x",
                            dongle_usb_pkt_button_mask);
            dongle_usb_pkt_button_mask = 0;
        }
    }
    dongle_usb_need_check_release = false;
    return;
}

static bool dongle_ppt_trans_handle_cfg(T_PPT_TRANS_APPLICATION_DATA request, void *data,
                                        uint16_t *len)
{
    switch (request)
    {
    case PPT_TRANS_APPLICATION_LINK_LOST:
        {
            dongle_ppt_send_release_button_pkt();
            dongle_ppt_trans_sync_buffer_x = 0;
            dongle_ppt_trans_sync_buffer_y = 0;

            break;
        }

    default:
        {
            *len = 0;
            break;
        }
    }
    return true;
}

static void dongle_ppt_trans_parse_usb_pkt(uint8_t seq_num, uint8_t pkt_num,
                                           T_PPT_TRANS_MOUSE_DATA *pkt_data)
{
    /** USB packet format
     *  Byte0: Button
     *  Byte1: lower byte of motion axis X
     *  Byte2: upper byte of motion axis X
     *  Byte3: lower byte of motion axis Y
     *  Byte4: upper byte of motion axis Y
     *  Byte5: vertical wheel offset
     *  Byte6: horizonal wheel offset
     *
     * note: add byte7 to indicate packet is valid or not
     */
    seq_num = (uint8_t)(seq_num - (pkt_num - 1)) % BIT(PPT_PKT_PAYLOAD_SIZE_SEQ);
    uint8_t button_mask = (BIT(PPT_PKT_PAYLOAD_SIZE_BUTTON) - 1);
    for (uint8_t i = 0; i < pkt_num; i++)
    {
        uint8_t pkt_no = (seq_num + i) % DONGLE_PPT_TRANS_HIST_PKT_NUM;
        dongle_usb_pkt_table[pkt_no][0] = pkt_data[pkt_num - 1].button & button_mask;
        dongle_usb_pkt_table[pkt_no][1] = pkt_data[i].optical_x;
        dongle_usb_pkt_table[pkt_no][2] = ((dongle_usb_pkt_table[pkt_no][1] & 0x80) == 0) ? 0 : 0xff;
        dongle_usb_pkt_table[pkt_no][3] = pkt_data[i].optical_y;
        dongle_usb_pkt_table[pkt_no][4] = ((dongle_usb_pkt_table[pkt_no][3] & 0x80) == 0) ? 0 : 0xff;

        if (i == (pkt_num - 1))
        {
            switch (pkt_data[i].wheel_direction)
            {
            case PPT_TRANS_WHEEL_V_UP_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x01;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                    break;
                }

            case PPT_TRANS_WHEEL_V_DOWN_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0xFF;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                    break;
                }

            case PPT_TRANS_WHEEL_H_UP_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0x01;
                    break;
                }

            case PPT_TRANS_WHEEL_H_DOWN_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0xFF;
                    break;
                }
            case PPT_TRANS_WHEEL_RELEASE_DEF:
            default:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                }
                break;
            }
        }
        else
        {
            dongle_usb_pkt_table[pkt_no][5] = 0x00;
            dongle_usb_pkt_table[pkt_no][6] = 0x00;
        }
        dongle_usb_pkt_table[pkt_no][USB_MOUSE_DATA_LEN] = true;     // set as valid
    }
}
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
static void dongle_ppt_trans_parse_pos_usb_pkt(uint8_t seq_num, uint8_t pkt_num,
                                               T_PPT_MOUSE_ABS_POS_DATA *pkt_data)
{
    /** USB packet format
     *  Byte0: Button
     *  Byte1: lower byte of motion axis X
     *  Byte2: upper byte of motion axis X
     *  Byte3: lower byte of motion axis Y
     *  Byte4: upper byte of motion axis Y
     *  Byte5: vertical wheel offset
     *  Byte6: horizonal wheel offset
     *
     * note: add byte7 to indicate packet is valid or not
     */
    seq_num = (uint8_t)(seq_num - (pkt_num - 1)) % BIT(PPT_PKT_PAYLOAD_SIZE_SEQ);
    uint8_t button_mask = (BIT(PPT_PKT_PAYLOAD_SIZE_BUTTON) - 1);
    for (uint8_t i = 0; i < pkt_num; i++)
    {
        uint8_t pkt_no = (seq_num + i) % DONGLE_PPT_TRANS_HIST_PKT_NUM;
        dongle_usb_pkt_table[pkt_no][0] = pkt_data[pkt_num - 1].button & button_mask;
        dongle_usb_pkt_table[pkt_no][1] = pkt_data[i].optical_x & 0xff;
        dongle_usb_pkt_table[pkt_no][2] = (pkt_data[i].optical_x >> 8);
        dongle_usb_pkt_table[pkt_no][3] = pkt_data[i].optical_y & 0xFF;
        dongle_usb_pkt_table[pkt_no][4] = (pkt_data[i].optical_y >> 8);
        dongle_pos_table[pkt_no][0] = pkt_data[i].optical_x;
        dongle_pos_table[pkt_no][1] = pkt_data[i].optical_y;
        if (i == (pkt_num - 1))
        {
            switch (pkt_data[i].wheel_direction)
            {
            case PPT_TRANS_WHEEL_V_UP_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x01;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                    break;
                }

            case PPT_TRANS_WHEEL_V_DOWN_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0xFF;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                    break;
                }

            case PPT_TRANS_WHEEL_H_UP_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0x01;
                    break;
                }

            case PPT_TRANS_WHEEL_H_DOWN_DEF:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0xFF;
                    break;
                }
            case PPT_TRANS_WHEEL_RELEASE_DEF:
            default:
                {
                    dongle_usb_pkt_table[pkt_no][5] = 0x00;
                    dongle_usb_pkt_table[pkt_no][6] = 0x00;
                }
                break;
            }
        }
        else
        {
            dongle_usb_pkt_table[pkt_no][5] = 0x00;
            dongle_usb_pkt_table[pkt_no][6] = 0x00;
        }
        dongle_usb_pkt_table[pkt_no][USB_MOUSE_DATA_LEN] = true;     // set as valid

    }
}
#endif
static void dongle_ppt_trans_long_pkt_recv_cb(bool result, uint8_t *pkt_data,
                                              uint16_t len, sync_receive_info_t info)
{
    APP_PRINT_INFO2("[dongle_ppt_trans_long_pkt_recv_cb] result: %d, data: [%b]",
                    result, TRACE_BINARY(len, pkt_data));
}

static void dongle_ppt_trans_get_usb_pkt(uint8_t *start_index, uint8_t *pkts_to_send)
{
    if (dongle_usb_pkt_table_read_idx == dongle_usb_pkt_table_write_idx)
    {
        *pkts_to_send = 0;
        return;
    }

    uint8_t delta_num = (DONGLE_PPT_TRANS_HIST_PKT_NUM + dongle_usb_pkt_table_write_idx -
                         dongle_usb_pkt_table_read_idx) %
                        DONGLE_PPT_TRANS_HIST_PKT_NUM;
    if (delta_num >= 2)
    {
        if (delta_num > PPT_PKT_PAYLOAD_DATA_NUM_MAX)
        {
            *pkts_to_send = PPT_PKT_PAYLOAD_DATA_NUM_MAX;
            *start_index = (DONGLE_PPT_TRANS_HIST_PKT_NUM + dongle_usb_pkt_table_write_idx -
                            PPT_PKT_PAYLOAD_DATA_NUM_MAX) %
                           DONGLE_PPT_TRANS_HIST_PKT_NUM;
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
            dongle_ppt_trans_miss_cnt[PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1]++;
#endif
            return;
        }
    }

    uint8_t temp_read_idx = dongle_usb_pkt_table_read_idx;
    while (temp_read_idx != dongle_usb_pkt_table_write_idx)
    {
        // find first valid history packet to resend
        if (dongle_usb_pkt_table[(temp_read_idx % DONGLE_PPT_TRANS_HIST_PKT_NUM)][USB_MOUSE_DATA_LEN] ==
            true)
        {
            break;
        }
        temp_read_idx = (temp_read_idx + 1) % DONGLE_PPT_TRANS_HIST_PKT_NUM;
    }
    *pkts_to_send = (DONGLE_PPT_TRANS_HIST_PKT_NUM + dongle_usb_pkt_table_write_idx - temp_read_idx) %
                    DONGLE_PPT_TRANS_HIST_PKT_NUM;
    *start_index = temp_read_idx;
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
    uint8_t record_pkt_send = ((*pkts_to_send) - 1) % PPT_PKT_PAYLOAD_DATA_NUM_MAX;
    dongle_ppt_trans_miss_cnt[record_pkt_send]++;
#endif
    return;
}

static void dongle_ppt_trans_receive_msg_cb(T_PPT_TRANS_MOUSE_DATA *data, uint8_t len,
                                            uint8_t seq_num)
{
    dongle_ppt_trans_parse_usb_pkt(seq_num, len, data);
    dongle_ppt_update_write_idx(seq_num + 1);

    uint8_t start_index, pkts_send;
    dongle_ppt_trans_get_usb_pkt(&start_index, &pkts_send);
    while (pkts_send --)
    {
        uint8_t *pkt_content = dongle_usb_pkt_table[start_index];
        start_index = (uint8_t)(start_index + 1) % DONGLE_PPT_TRANS_HIST_PKT_NUM;         // jump to next

        if (pkt_content[USB_MOUSE_DATA_LEN] == false)
        {
            APP_PRINT_INFO0("[dongle_ppt_trans_receive_msg_cb] invalid packet");
            continue;
        }
        uint32_t s = os_lock();

        int16_t temp_x, temp_y;
        bool compensated = false;
        temp_x = (int16_t)((pkt_content[2] << 8) | pkt_content[1]);
        temp_y = (int16_t)((pkt_content[4] << 8) | pkt_content[3]);
        if (dongle_ppt_trans_sync_buffer_x != 0)
        {
            int16_t temp_buffer = dongle_ppt_trans_sync_buffer_x;
            dongle_ppt_trans_sync_buffer_x = 0;
            temp_x += temp_buffer;
            compensated = true;
        }

        if (dongle_ppt_trans_sync_buffer_y != 0)
        {
            int16_t temp_buffer = dongle_ppt_trans_sync_buffer_y;
            dongle_ppt_trans_sync_buffer_y = 0;
            temp_y += temp_buffer;
            compensated = true;
        }

        if (true == compensated)
        {
            uint16_t temp_uint_x, temp_uint_y;
            temp_uint_x = (uint16_t) temp_x;
            temp_uint_y = (uint16_t) temp_y;
            pkt_content[1] = temp_uint_x & 0xFF;
            pkt_content[2] = temp_uint_x >> 8;
            pkt_content[3] = temp_uint_y & 0xFF;
            pkt_content[4] = temp_uint_y >> 8;
        }

        pkt_content[USB_MOUSE_DATA_LEN] = false;           // set as handled

        if (true == ppt_app_global_data.is_enable_to_receive_data)
        {
            if (false == app_usb_send_mouse_data(pkt_content))
            {
                dongle_ppt_trans_sync_buffer_x += temp_x;
                dongle_ppt_trans_sync_buffer_y += temp_y;
            }
            else
            {
                dongle_usb_pkt_button_mask = pkt_content[0];

#if DONGLE_PPT_TRANS_PRINT_STATS_EN
                dongle_ppt_trans_usb_send_cnt++;
#endif
            }
        }
        os_unlock(s);
    }
    dongle_ppt_update_read_idx(start_index);
}
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#define MIN(x, y) (x<y?x:y)
#define abs(x) (x < 0 ? -x:x)

static void dongle_ppt_trans_receive_pos_msg_cb(T_PPT_MOUSE_ABS_POS_DATA *data, uint8_t len,
                                                uint8_t seq_num)
{
    uint32_t s = os_lock();
    dongle_ppt_trans_parse_pos_usb_pkt(seq_num, len, data);
    dongle_ppt_update_write_idx(seq_num + 1);
    uint8_t start_index, pkts_send;

    dongle_ppt_trans_get_usb_pkt(&start_index, &pkts_send);
    pkts_send = len;
    start_index = ((seq_num - (len - 1)) % BIT(PPT_PKT_PAYLOAD_SIZE_SEQ)  %
                   DONGLE_PPT_TRANS_HIST_PKT_NUM);
    os_unlock(s);

    while (pkts_send --)
    {
        uint32_t s = os_lock();
        uint8_t *pkt_content = dongle_usb_pkt_table[start_index];
        int32_t temp_x = 0, temp_y = 0;
        int32_t pos_x = 0, pos_y = 0;

        temp_x = dongle_pos_table[start_index][0];
        temp_y = dongle_pos_table[start_index][1];
        if (len != 1)
        {
            start_index = (uint8_t)(start_index + 1) % DONGLE_PPT_TRANS_HIST_PKT_NUM;    // jump to next
        }

        if (pkt_content[USB_MOUSE_DATA_LEN] == false)
        {
            APP_PRINT_INFO4("[dongle_ppt_trans_receive_msg_cb] invalid packet, %d %d %d %d", len, seq_num,
                            start_index, pkts_send);
            os_unlock(s);
            continue;
        }
        pos_x = temp_x;
        pos_y = temp_y;

        if ((pos_x >= 0 && dongle_ppt_trans_sync_buffer_x >= 0) || (pos_x < 0 &&
                                                                    dongle_ppt_trans_sync_buffer_x < 0))
        {
            temp_x -= dongle_ppt_trans_sync_buffer_x;
        }
        else if (pos_x >= 0)
        {
            int32_t can1, can2;
            can1 = pos_x + (- dongle_ppt_trans_sync_buffer_x);
            can2 = ((-PPT_TRANS_VECTOR_SUM_POS_VAL_MIN) + dongle_ppt_trans_sync_buffer_x) +
                   (PPT_TRANS_VECTOR_SUM_POS_VAL_MAX - pos_x) + 1;
            if (MIN(abs(can1), abs(can2)) == abs(can1))
            {
                temp_x = can1;
            }
            else
            {
                temp_x = can2;
            }
        }
        else
        {
            int32_t can1, can2;
            can1 = -(dongle_ppt_trans_sync_buffer_x + (-temp_x));
            can2 = ((-PPT_TRANS_VECTOR_SUM_POS_VAL_MIN) + temp_x) + (PPT_TRANS_VECTOR_SUM_POS_VAL_MAX -
                                                                     dongle_ppt_trans_sync_buffer_x) +
                   1;

            if (MIN(abs(can1), abs(can2)) == abs(can1))
            {
                temp_x = can1;
            }
            else
            {
                temp_x = can2;
            }
        }

        if ((pos_y >= 0 && dongle_ppt_trans_sync_buffer_y >= 0) || (pos_y < 0 &&
                                                                    dongle_ppt_trans_sync_buffer_y < 0))
        {
            temp_y -= dongle_ppt_trans_sync_buffer_y;
        }
        else if (pos_y >= 0)
        {
            int32_t can1, can2;
            can1 = temp_y + (- dongle_ppt_trans_sync_buffer_y);
            can2 = ((-PPT_TRANS_VECTOR_SUM_POS_VAL_MIN) + dongle_ppt_trans_sync_buffer_y) +
                   (PPT_TRANS_VECTOR_SUM_POS_VAL_MAX - temp_y) +
                   1;

            if (MIN(abs(can1), abs(can2)) == abs(can1))
            {
                temp_y = can1;
            }
            else
            {
                temp_y = can2;
            }
        }
        else
        {
            int32_t can1, can2;
            can1 = -(dongle_ppt_trans_sync_buffer_y + (-temp_y));
            can2 = ((-PPT_TRANS_VECTOR_SUM_POS_VAL_MIN) + temp_y) + (PPT_TRANS_VECTOR_SUM_POS_VAL_MAX -
                                                                     dongle_ppt_trans_sync_buffer_y) +
                   1;
            if (MIN(abs(can1), abs(can2)) == abs(can1))
            {
                temp_y = can1;
            }
            else
            {
                temp_y = can2;
            }
        }

        pkt_content[1] = temp_x & 0xff;
        pkt_content[2] = temp_x >> 8 ;
        pkt_content[3] = temp_y & 0xff;
        pkt_content[4] = temp_y >> 8 ;

        pkt_content[USB_MOUSE_DATA_LEN] = false;           // set as handled


        if (true == ppt_app_global_data.is_enable_to_receive_data)
        {
            PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_PPT_TRANSPORT_USB_MSG_SEND);
            if (true == app_usb_send_mouse_data(pkt_content))
            {
                /** update absolute position */
                dongle_ppt_trans_sync_buffer_x = pos_x;
                dongle_ppt_trans_sync_buffer_y = pos_y;
                dongle_usb_pkt_button_mask = pkt_content[0];
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
                dongle_ppt_trans_usb_send_cnt++;
#endif
            }
            PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_PPT_TRANSPORT_USB_MSG_SEND);
        }
        os_unlock(s);
    }
    dongle_ppt_update_read_idx(start_index);
}
#endif

#if DONGLE_PPT_TRANS_PRINT_STATS_EN
static void dongle_ppt_trans_dump_stats_cb(void *p_timer)
{
    uint32_t temp_usb_send_cnt = 0;
    uint16_t temp_miss_cnt[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};

    uint32_t s = os_lock();
    temp_usb_send_cnt = dongle_ppt_trans_usb_send_cnt;
    dongle_ppt_trans_usb_send_cnt = 0;
    memcpy(temp_miss_cnt, dongle_ppt_trans_miss_cnt, sizeof(dongle_ppt_trans_miss_cnt));
    memset(dongle_ppt_trans_miss_cnt, 0, sizeof(dongle_ppt_trans_miss_cnt));
    os_unlock(s);

    /** note: should be changed when PPT_PKT_PAYLOAD_DATA_NUM_MAX is modified */
    APP_PRINT_INFO3("[dongle_ppt_trans_dump_stats_cb] USB Send cnt: %d, adopted: (%d, %d)",
                    temp_usb_send_cnt, temp_miss_cnt[0], temp_miss_cnt[1]);
}
#endif // DONGLE_PPT_TRANS_PRINT_STATS_EN

void dongle_ppt_trans_handle_set_check_release(void)
{
    dongle_usb_need_check_release = true;
}

void dongle_ppt_trans_handle_sync_evt_hook(sync_event_t event)
{
    if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT) == false)
    {
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
        os_timer_stop(&dongle_ppt_trans_dump_stats_timer);
#endif
        return;
    }

    switch (event)
    {
    case SYNC_EVENT_CONNECTED:
        {
            if (dongle_usb_need_check_release)
            {
                dongle_ppt_send_release_button_pkt();
            }
#if DONGLE_PPT_TRANS_PRINT_STATS_EN
            os_timer_restart(&dongle_ppt_trans_dump_stats_timer, DONGLE_PPT_TRANS_PRINT_STATS_ITVL);
#endif // DONGLE_PPT_TRANS_PRINT_STATS_EN
            break;
        }
    case SYNC_EVENT_CONNECT_LOST:
        {
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
            ppt_trans_chann_ctrl_slave_handle_disconnect();
#endif
            break;
        }
    default:
        break;
    }

    ppt_trans_handle_sync_event_cb_hook(event);
}

void dongle_ppt_trans_handle_init(void)
{
    T_PPT_TRANS_INIT_TYPEDEF ppt_trans_cfg = {0};
    ppt_trans_handle_struct_init(&ppt_trans_cfg);
    ppt_trans_cfg.ppt_trans_role = PPT_TRANS_ROLE_SLAVE;
    ppt_trans_cfg.ppt_trans_app_rx_handler = dongle_ppt_trans_receive_msg_cb;
    ppt_trans_cfg.ppt_trans_app_rx_raw_handler = dongle_ppt_trans_rx_raw_cb;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_cfg.ppt_trans_app_pos_rx_handler = dongle_ppt_trans_receive_pos_msg_cb;
#endif
    ppt_trans_cfg.ppt_trans_app_req_handler = dongle_ppt_trans_handle_cfg;
    ppt_trans_cfg.ppt_trans_app_feature_mask = \
                                               BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_PKT_RETRANS) |
                                               BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_POS_CTRL) |
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
                                               BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_CHAN_CTRL) |
#endif
                                               BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT);
    ppt_trans_handle_init(&ppt_trans_cfg);
    dongle_usb_pkt_button_mask = 0;
    dongle_usb_need_check_release = false;

#if DONGLE_PPT_TRANS_PRINT_STATS_EN
    if (false == os_timer_create(&dongle_ppt_trans_dump_stats_timer,
                                 "usb recv stat timer", 0, DONGLE_PPT_TRANS_PRINT_STATS_ITVL,
                                 true, dongle_ppt_trans_dump_stats_cb))
    {
        APP_PRINT_ERROR0("[dongle_ppt_trans_handle_init] stat timer creation failed");
    }
    else
    {
        dongle_ppt_trans_usb_send_cnt = 0;
        memset(dongle_ppt_trans_miss_cnt, 0, sizeof(dongle_ppt_trans_miss_cnt));
    }
#endif

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
    ppt_trans_handle_long_pkt_recv(dongle_ppt_trans_long_pkt_recv_cb);
#endif
}

#endif // FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
