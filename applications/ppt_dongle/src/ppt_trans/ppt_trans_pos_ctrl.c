/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pos_pkt_ctrl.c
   * @brief     mouse position data control module
   * @author
   * @date
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "board.h"
#include "stdint.h"
#include "string.h"
#include "trace.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_pos_ctrl.h"
#include "ppt_trans_pkt_algo.h"
#include "app_section.h"
#include "ppt_sync.h"

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#define PPT_TRANS_8K_CE_TOR       ((PPT_PKT_PAYLOAD_SEQ_NUM_SIZE >> 4) + 1)
#define PPT_TRANS_NON_8K_CE_TOR   (PPT_TRANS_8K_CE_TOR << 3)
#define MAX(x,y) x > y ? x : y
#define U32_MINUS(a, b) (a > b ? (a - b) : (UINT32_MAX - b + a))
/*PPT_PKT_PAYLOAD_SEQ_NUM_SIZE (64) - 1 (SEQ start from 0) -
  FIFO_DEPTH (2) - AIR (1) - VECTOR_PER_PACKET (4) -
    Fail safe Tolerence (11) = 40. If increase the Fail safe tolerence,
    the system will more likely to send position packet*/
#define PPT_TRANS_FORCE_POS_SEQ_GAP 40

typedef struct
{
    uint8_t seq : 6;
    uint8_t temp : 2;
} SEQ_NUM;

typedef struct
{
    bool valid;
    uint32_t u32_seq;
} SEQ_REC;
/*============================================================================*
 *                              Declarations
 *============================================================================*/
void ppt_trans_pos_ctrl_record_pos(uint8_t idx);
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_get_hist_pos(uint8_t idx);
void ppt_trans_pos_ctrl_update_pos(T_PPT_TRANS_POS_VECTOR_SUM new_pos);
void ppt_trans_pos_ctrl_update_recv_ankor(uint8_t seq, uint16_t ce);
uint8_t ppt_trans_pos_ctrl_get_prev_seq(void);
uint8_t ppt_trans_pos_ctrl_judge_pkt_valid(uint8_t seq, uint32_t ce_cnt,
                                           uint8_t range);
bool ppt_trans_pos_ctrl_judge_force_pos_packet(uint8_t seq, uint32_t u32_seq);
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_recv_position_vector(int16_t x,
                                                                   int16_t y);
void ppt_trans_pos_ctrl_handle_recv_msg(uint8_t *p_data, uint16_t len,
                                        sync_receive_info_t *info);
void ppt_trans_pos_ctrl_handle_ack(uint8_t seq, bool is_pos_packet);
void ppt_trans_pos_ctrl_handle_nack(uint8_t seq, bool is_pos_packet);
void ppt_trans_pos_ctrl_handle_rec_seq(uint8_t seq, uint32_t u32_seq);
/*============================================================================*
 *                              Static Variables
 *============================================================================*/
static uint8_t prev_seq = 0;
static uint32_t prev_ce = UINT32_MAX;
static uint64_t discon_ankor = UINT64_MAX;
static T_PPT_TRANS_POS_VECTOR_SUM pos;
static T_PPT_TRANS_POS_VECTOR_SUM hist_pos[PPT_PKT_HIST_PKT_SIZE] = {0};
static ppt_trans_handle_receive_pos_pkt_cb pos_callback_fp = NULL;
static uint8_t last_ack_seq = 0xFF;
static uint8_t normal_ack_seq = 0xFF;
static uint8_t prev_button = 0;
static uint8_t force_pos_gap = 0;
static uint32_t prev_u32_seq = UINT32_MAX;
SEQ_REC u32_seq_rec_arr[PPT_PKT_PAYLOAD_SEQ_NUM_SIZE];

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  store - the currently position to the hist_pos array for future usage.
 * @param  idx - the recording index of the hist_pos array
 * @retval none
 */
void ppt_trans_pos_ctrl_record_pos(uint8_t idx)
{
    memcpy(hist_pos + idx, &pos,
           sizeof(pos));
}

/******************************************************************
 * @brief  get the previous record position which store in the hist_pos array.
 * @param  idx - the recording index of the hist_pos array
 * @return the position record of the previous moment.
 * @retval T_PPT_TRANS_POS_VECTOR_SUM
 */
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_get_hist_pos(uint8_t idx)
{
    return hist_pos[idx];
}

/******************************************************************
 * @brief  update the record position directly.
 * @param  new_pos - the new position.
 * @retval none
 */
void ppt_trans_pos_ctrl_update_pos(T_PPT_TRANS_POS_VECTOR_SUM new_pos)
{
    pos = new_pos;
}

/******************************************************************
 * @brief  set position control force position parameter.
 * @param  msg_quota - the msg quota of sync lib
 * @retval none
 */
void ppt_trans_pos_ctrl_set_pos_pos_seq_gap(uint8_t msg_quota)
{
    force_pos_gap = msg_quota + 1;
}


/******************************************************************
 * @brief  update record packet receiving anchor directly.
 * @param  seq - the sequence number of the received packet
 * @param  ce - the time anchor when receiving packet
 * @retval none
 */
void ppt_trans_pos_ctrl_update_recv_ankor(uint8_t seq, uint16_t ce)
{
    prev_ce = ce;
    prev_seq = seq;
}

/******************************************************************
 * @brief   update the record positon based on the received displacement vector.
 * @param   x - the position displacement on x-axis
 * @param   y - the position displacement on y-axis
 * @return  the updated position.
 * @retval  T_PPT_TRANS_POS_VECTOR_SUM
 */
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_recv_position_vector(int16_t x, int16_t y)
{
    pos.pos_x += x;
    pos.pos_y += y;
    if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV) == false)
    {
        int16_t temp_pos_x = (int16_t)pos.pos_x, temp_pos_y = (int16_t)pos.pos_y;
        pos.pos_x = (int32_t)temp_pos_x;
        pos.pos_y = (int32_t)temp_pos_y;
    }
    else
    {
        pos.pos_x &= INT24_MASK;
        pos.pos_y &= INT24_MASK;
    }
    return pos;
}

/******************************************************************
 * @brief   get the sequence number of previous valid received packet
 * @return  sequence number of packet
 * @retval  uint8_t
 */
uint8_t ppt_trans_pos_ctrl_get_prev_seq(void)
{
    return prev_seq;
}

/******************************************************************
 * @brief  judge whether the received packet is consecutively to prevent misalignment.
 * @param  seq - the sequence number of the received packet
 * @param  ce_cnt - the time anchor when receiving packet
 * @param  range - the tolerance for sequence skip
 * @return the validation result of the packet, 0 is invalid otherwise valid packet
 * @retval uint8_t
 */
uint8_t ppt_trans_pos_ctrl_judge_pkt_valid(uint8_t seq, uint32_t ce_cnt, uint8_t range)
{
    uint8_t ce_tor = PPT_TRANS_8K_CE_TOR;
    if (ppt_trans_handle_cfg_mgr.report_rate != 8000)
    {
        ce_tor = PPT_TRANS_NON_8K_CE_TOR;
    }

    for (uint8_t i = 1; i <= range; i++)
    {
        SEQ_NUM temp;
        temp.seq = prev_seq + i;
        if (seq == temp.seq && ((prev_ce == UINT32_MAX || ce_cnt == UINT32_MAX) ||
                                (ce_cnt >= prev_ce && ce_cnt - prev_ce <= ce_tor) ||
                                (ce_cnt < prev_ce && UINT16_MAX + ce_cnt - prev_ce <= ce_tor)))
        {
            prev_seq = seq;
            prev_ce = ce_cnt;
            return i;
        }
    }
    return 0;
}

/******************************************************************
 * @brief  reset record position history data which store in the hist_pos array.
 * @return none
 * @retval void
 */
void ppt_trans_pos_ctrl_clear_hist(void)
{
    memset(hist_pos, 0, sizeof(hist_pos));
}

/******************************************************************
 * @brief  reset the current position.
 * @return none
 * @retval void
 */
void ppt_trans_pos_ctrl_clear_pos(void)
{
    memset(&pos, 0, sizeof(pos));
}

/******************************************************************
 * @brief  position control module init, reset the parameter.
 * @return none
 * @retval void
 */
void ppt_trans_pos_ctrl_init(void)
{
    APP_PRINT_TRACE0("[ppt_trans_pos_ctrl_init] position reset");
    memset(u32_seq_rec_arr, 0, sizeof(u32_seq_rec_arr));
    prev_button = 0;
    ppt_trans_pos_ctrl_clear_pos();
    ppt_trans_pos_ctrl_clear_hist();
}

/******************************************************************
 * @brief   handle positon control module when 2.4G master and slave disconnected.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_disconnect(void)
{
    discon_ankor = os_sys_time_get();
    prev_ce = 0xFFFFFFF; //7 F means trust no one, used for 2.4G slave
    last_ack_seq = 0xFF;
}


/******************************************************************
 * @brief   handle positon control module when 2.4G master and slave connected.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_connect(void)
{
    uint64_t cur = os_sys_time_get();
    uint64_t diff = cur >= discon_ankor ? cur - discon_ankor : (UINT64_MAX - discon_ankor) + cur;
    if (diff >= PPT_PKT_LINK_LOST_FLUSH_TIME)
    {
        ppt_trans_pos_ctrl_init();
    }
}

bool ppt_trans_pos_ctrl_judge_force_pos_packet(uint8_t seq, uint32_t u32_seq)
{
    bool force_pos = false;
    uint8_t seq_max = PPT_PKT_PAYLOAD_SEQ_NUM_SIZE;


    if (ppt_trans_handle_cfg_mgr.report_rate != 8000)
    {
        uint8_t normal_seq_gap = MAX(force_pos_gap, PPT_PKT_PAYLOAD_DATA_NUM_MAX);
        if (normal_ack_seq != 0xFF)
        {
            if ((seq > normal_ack_seq &&
                 (seq - normal_ack_seq > normal_seq_gap) ||
                 (seq <= normal_ack_seq &&
                  (seq_max - normal_ack_seq + seq > normal_seq_gap))))
            {
                force_pos = true;
            }
        }
        else
        {
            force_pos = true;
        }
    }

    if (last_ack_seq != 0xFF)
    {
        if ((seq > last_ack_seq &&
             (seq - last_ack_seq > 16) ||
             (seq <= last_ack_seq &&
              (seq_max - last_ack_seq + seq > 16))))
        {
            force_pos = true;
        }
    }
    else
    {
        force_pos = true;
    }

    if (U32_MINUS(u32_seq, prev_u32_seq) > PPT_TRANS_FORCE_POS_SEQ_GAP)
    {
        force_pos = true;
    }
    return force_pos;
}

/******************************************************************
 * @brief  ppt_trans_pos_ctrl_handle_recv_msg
 * @param  p_data - pointer to receive data.
 * @param  len - data length.
 * @param  rssi - ble rssi of data
 * @return none
 * @retval void
 */
void ppt_trans_pos_ctrl_handle_recv_msg(uint8_t *p_data, uint16_t len,
                                        sync_receive_info_t *info)
{
    T_PPT_TRANS_MOUSE_DATA data_out[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
    T_PPT_MOUSE_ABS_POS_DATA data_to_usb[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
    T_PPT_TRANS_PKT_HEADER header;
    header.d8 = p_data[0];
    if (header.pkt_type == PPT_TRANS_PKT_TYPE_COMPENSATE)
    {
        uint32_t s = os_lock();
        T_PPT_TRANS_POS_VECTOR_SUM temp;
        uint8_t u16_container[4];
        memset(&u16_container, 0, sizeof(u16_container));
        uint8_t u24_container[6];
        memset(&u24_container, 0, sizeof(u24_container));
        memset(&temp, 0, sizeof(temp));
        uint8_t seq_num;
        uint8_t length = 0;

        if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV) == false)
        {
            // position date is 16bits when usging 8bits motion data
            uint8_t length_pos = 2 * 2;
            ppt_trans_handle_cfg_mgr.ppt_trans_algo_parse_pos_handler(p_data, len,
                                                                      (uint8_t *)(&u16_container), length_pos,
                                                                      data_out, &length,
                                                                      &seq_num);
            temp.pos_x = (int32_t)((u16_container[0] | u16_container[1] << 8));
            temp.pos_y = (int32_t)((u16_container[2] | u16_container[3] << 8));
        }
        else
        {
            // position date is 24bits when usging 16bits motion data
            uint8_t length_pos = 3 * 2;
            ppt_trans_handle_cfg_mgr.ppt_trans_algo_parse_pos_handler(p_data, len,
                                                                      (uint8_t *)(&u24_container), length_pos,
                                                                      data_out, &length,
                                                                      &seq_num);
            temp.pos_x = (int32_t)((u24_container[0] | u24_container[1] << 8 | u24_container[2] << 16)&
                                   INT24_MASK);
            temp.pos_y = (int32_t)((u24_container[3] | u24_container[4] << 8 | u24_container[5] << 16) &
                                   INT24_MASK);
        }

//        if (length == 0)
//        {
//            os_unlock(s);
//            return;
//        }


        uint8_t local_prev_seq = ppt_trans_pos_ctrl_get_prev_seq();
//        bool valid = judge_pos_valid(seq_num, info->ce_count, length + 1);
//        if (!valid)
//        {
//            os_unlock(s);
//            return;
//        }
        ppt_trans_pos_ctrl_update_recv_ankor(seq_num, info->ce_count);

        os_unlock(s);
        memset(data_to_usb, 0, sizeof(data_to_usb));
        uint8_t send_len = 1;

        if (length == 0)
        {
            data_to_usb[0].button = data_out[0].button;
            data_to_usb[0].wheel_direction = data_out[0].wheel_direction;
            data_to_usb[0].optical_x = temp.pos_x;
            data_to_usb[0].optical_y = temp.pos_y;
            prev_button = data_out[0].button;
        }
        else if ((local_prev_seq == (PPT_PKT_PAYLOAD_SEQ_NUM_SIZE - 1) && seq_num == 0) ||
                 (local_prev_seq + 1 == seq_num) ||
                 (PPT_PKT_PAYLOAD_DATA_NUM_MAX == 1))
        {
            data_to_usb[0].button = data_out[length - 1].button;
            data_to_usb[0].wheel_direction = data_out[length - 1].wheel_direction;
            data_to_usb[0].optical_x = temp.pos_x;
            data_to_usb[0].optical_y = temp.pos_y;
            prev_button = data_out[length - 1].button;
        }
        else
        {
            SEQ_NUM temp_seq;
            temp_seq.seq = local_prev_seq;
            uint8_t to_send_len = temp_seq.seq != seq_num ? 0 : length + 1;
            T_PPT_MOUSE_ABS_POS_DATA temp_data_to_usb[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
            memset(temp_data_to_usb, 0, sizeof(temp_data_to_usb));
            send_len = 0;
            uint8_t w_idx = temp_seq.seq != seq_num ? 0 : length + 1;
            uint8_t total_length = length + 1;
            while (temp_seq.seq != seq_num && w_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV && total_length > 0)
            {
                to_send_len++;
                temp_seq.seq++;
                total_length--;
                w_idx++;
            }

            temp_seq.seq = local_prev_seq;
            w_idx--;
            data_to_usb[w_idx].button = data_out[length - 1].button;
            data_to_usb[w_idx].wheel_direction = data_out[length - 1].wheel_direction;
            data_to_usb[w_idx].optical_x = temp.pos_x;
            data_to_usb[w_idx].optical_y = temp.pos_y;
            T_PPT_TRANS_POS_VECTOR_SUM vec_pos;
            vec_pos.pos_x = temp.pos_x;
            vec_pos.pos_y = temp.pos_y;

            temp_data_to_usb[w_idx].button = data_out[length - 1].button;
            temp_data_to_usb[w_idx].wheel_direction = data_out[length - 1].wheel_direction;
            temp_data_to_usb[w_idx].optical_x = temp.pos_x;
            temp_data_to_usb[w_idx].optical_y = temp.pos_y;

            send_len = 1;
            to_send_len--;
            w_idx--;
            uint8_t vec_idx = length - 1;

            while (to_send_len > 0 && w_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV)
            {
//                if (!((int16_t)data_out[vec_idx].optical_x == 0 &&
//                    (int16_t)data_out[vec_idx].optical_y == 0))
                {
                    vec_pos.pos_x = vec_pos.pos_x - (int16_t)data_out[vec_idx].optical_x;
                    vec_pos.pos_y = vec_pos.pos_y - (int16_t)data_out[vec_idx].optical_y;
                    temp_data_to_usb[w_idx].button = data_out[length - 1].button;
                    temp_data_to_usb[w_idx].wheel_direction = data_out[length - 1].wheel_direction;
                    temp_data_to_usb[w_idx].optical_x = vec_pos.pos_x;
                    temp_data_to_usb[w_idx].optical_y = vec_pos.pos_y;
                    send_len++;
                    w_idx--;
                }

                vec_idx--;
                to_send_len--;
            }

            T_PPT_TRANS_POS_VECTOR_SUM prev;
            prev.pos_x = temp_data_to_usb[0].optical_x - 1;
            prev.pos_y = temp_data_to_usb[0].optical_y - 1;
            w_idx = 0;
            for (uint8_t i = 0; i < send_len; i++)
            {
                if (prev.pos_x != temp_data_to_usb[i].optical_x || prev.pos_y != temp_data_to_usb[i].optical_y)
                {
                    data_to_usb[w_idx].button = temp_data_to_usb[i].button;
                    data_to_usb[w_idx].wheel_direction = temp_data_to_usb[i].wheel_direction;
                    data_to_usb[w_idx].optical_x = temp_data_to_usb[i].optical_x;
                    data_to_usb[w_idx].optical_y = temp_data_to_usb[i].optical_y;
                    prev.pos_x = temp_data_to_usb[i].optical_x;
                    prev.pos_y = temp_data_to_usb[i].optical_y;
                    w_idx++;
                }
            }

            send_len = w_idx;
            prev_button = data_out[length - 1].button;
        }

        ppt_trans_pos_ctrl_update_pos(temp);

        if ((pos_callback_fp != NULL) && length > 0)
        {
            pos_callback_fp(data_to_usb, send_len, seq_num);
        }
        return;
    }

    uint8_t length = 0;
    uint8_t seq_num;
    uint8_t used_bytes = 0;
    ppt_trans_pkt_algo_parse_pkt(p_data, len, data_out, &length, &seq_num, &used_bytes);

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
    if (header.long_pkt_en == PPT_TRANS_FIELD_ENABLE &&
        header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
    {
        ppt_trans_long_pkt_parse_pkt(p_data + used_bytes, len - used_bytes, *info);
    }
#endif

    if (length == 0)
    {
        return;
    }

    uint8_t range = length;

    uint8_t valid_vector = ppt_trans_pos_ctrl_judge_pkt_valid(seq_num, info->ce_count, range);
    T_PPT_TRANS_POS_VECTOR_SUM d_pos;
    if (valid_vector != 0)
    {

        memset(data_to_usb, 0, sizeof(data_to_usb));
        uint8_t valid_idx = length - valid_vector;
        for (uint8_t i = 0; i < valid_vector; i++)
        {
            d_pos = ppt_trans_pos_ctrl_recv_position_vector((int16_t)data_out[valid_idx + i].optical_x,
                                                            (int16_t)data_out[valid_idx + i].optical_y);
            data_to_usb[i].optical_x = d_pos.pos_x;
            data_to_usb[i].optical_y = d_pos.pos_y;
            data_to_usb[i].button = data_out[valid_idx + i].button;
            data_to_usb[i].wheel_direction = data_out[valid_idx + i].wheel_direction;
        }

        prev_button = data_out[length - 1].button;
        if (pos_callback_fp != NULL)
        {
            pos_callback_fp(data_to_usb, valid_vector, seq_num);
        }
    }
    else if (length > 0)
    {
        d_pos = ppt_trans_pos_ctrl_recv_position_vector(0,
                                                        0);
        data_to_usb[0].button = data_out[length - 1].button;
        data_to_usb[0].wheel_direction = data_out[length - 1].wheel_direction;
        data_to_usb[0].optical_x = d_pos.pos_x;
        data_to_usb[0].optical_y = d_pos.pos_y;
        if (prev_button != data_out[length - 1].button ||
            data_out[length - 1].wheel_direction != 0)
        {
            if ((pos_callback_fp != NULL) && (length > 0))
            {
                pos_callback_fp(data_to_usb, 1, seq_num);
            }
            valid_vector = 1;
        }
        prev_button = data_out[length - 1].button;
    }
}

/******************************************************************
 * @brief register app callback for receiving data for position control module
 * @param  cb - callback function when packet received
*/
void ppt_trans_pos_ctrl_reg_receive_pos_cb(ppt_trans_handle_receive_pos_pkt_cb cb)
{
    pos_callback_fp = cb;
}

/******************************************************************
 * @brief   handle positon control module when 2.4G master recv slave ack.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_ack(uint8_t seq, bool is_pos_packet)
{
    normal_ack_seq = seq;

    if (is_pos_packet)
    {
        last_ack_seq = seq;
        prev_u32_seq = u32_seq_rec_arr[seq].u32_seq;
        u32_seq_rec_arr[seq].valid = false;
    }
}

/******************************************************************
 * @brief   handle positon control module when 2.4G master recv slave nack.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_nack(uint8_t seq, bool is_pos_packet)
{
    if (is_pos_packet)
    {
        u32_seq_rec_arr[seq].valid = false;
    }
}

/******************************************************************
 * @brief   record u32 sequence and u8 seqence for position control usage.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_rec_seq(uint8_t seq, uint32_t u32_seq)
{
    if (u32_seq_rec_arr[seq].valid)
    {
        u32_seq_rec_arr[seq].u32_seq = U32_MINUS(u32_seq, PPT_PKT_PAYLOAD_SEQ_NUM_SIZE);
    }
    else
    {
        u32_seq_rec_arr[seq].u32_seq = u32_seq;
    }
    u32_seq_rec_arr[seq].valid = true;
}

/**
 * @brief   handle positon control module when 2.4G report rate is changed.
 * @return  none
 */
void ppt_trans_pos_ctrl_handle_report_rate_change(void)
{
    ppt_trans_pos_ctrl_init();
}
#endif
