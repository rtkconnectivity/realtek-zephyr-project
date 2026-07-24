/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pkt_ctrl.c
   * @brief     mouse packet data control module
   * @author    luke
   * @date      2023-09-28
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
#include "trace.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "ppt_sync.h"
#include "app_section.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_pkt_ctrl.h"
#include "ppt_trans_pkt_algo.h"
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#endif
/*============================================================================*
 *                              Defines
 *============================================================================*/
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/** Optical offset data compensation resolution bit size */
#define PPT_PKT_COMPENSATE_RESOLUTION   PPT_TRANS_MOTION_SIZE_4BITS
#define PPT_PKT_COMPENSATE_INT_MAX      (BIT(BIT(PPT_PKT_COMPENSATE_RESOLUTION) - 1) - 1)
#define PPT_PKT_COMPENSATE_INT_MIN      (-1 * BIT(BIT(PPT_PKT_COMPENSATE_RESOLUTION) - 1))
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

/*============================================================================*
 *                              Static Variables
 *============================================================================*/
/** optical sensor writes to memory */
static volatile uint8_t  ppt_trans_pkt_ctrl_write_idx;
/** memory reads for on-air transmission */
static volatile uint8_t  ppt_trans_pkt_ctrl_read_idx;
/** current packet sequence num */
static volatile uint8_t  ppt_trans_pkt_ctrl_seq_cnt = 0;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
/** current packet u32 sequence num */
static volatile uint32_t  ppt_trans_pkt_ctrl_u32_seq_cnt = 0;
#endif
/** buffer for history packet storage */
static T_PPT_TRANS_MOUSE_DATA ppt_trans_pkt_ctrl_hist_list[PPT_PKT_HIST_PKT_SIZE] = {0};
/** previous button mask status */
static uint8_t  ppt_trans_pkt_ctrl_button_mask = 0;

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/** history x-axis offset that needs to be compensated */
static volatile int32_t  ppt_trans_pkt_ctrl_compensate_x = 0;
/** history y-axis offset that needs to be compensated */
static volatile int32_t  ppt_trans_pkt_ctrl_compensate_y = 0;
/** history QDEC wheel data that needs to be compensated */
static T_PPT_TRANS_WHEEL_DIRECTION
ppt_trans_pkt_ctrl_compensate_wheel[PPT_PKT_HIST_WHEEL_ARR_SIZE] =
{PPT_TRANS_WHEEL_RELEASE_DEF};
/** history QDEC wheel data array read index */
static uint8_t  ppt_trans_pkt_ctrl_hist_wheel_read_idx = 0;
/** history QDEC wheel data array write index */
static uint8_t  ppt_trans_pkt_ctrl_hist_wheel_write_idx = 0;
/** max compensate value available */
static int32_t  ppt_trans_pkt_ctrl_compensate_upper = PPT_PKT_COMPENSATE_INT_MAX;
/** min compensate value available */
static int32_t  ppt_trans_pkt_ctrl_compensate_lower = PPT_PKT_COMPENSATE_INT_MIN;
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

/*============================================================================*
 *                              Declarations
 *============================================================================*/
void ppt_trans_pkt_ctrl_receive_data(T_PPT_TRANS_MOUSE_DATA mouse_data, bool is_dummy);
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
void ppt_trans_pkt_ctrl_get_compensate_data(uint8_t *next_pkt, uint8_t *length,
                                            uint8_t *wheel_dir);
void ppt_trans_pkt_ctrl_clear_history(void);
uint32_t ppt_trans_pkt_ctrl_get_u32_seq(void);
uint8_t ppt_trans_pkt_ctrl_get_cur_seq(void);
#endif
void ppt_trans_pkt_ctrl_get_offset_data(uint8_t *next_pkt, uint8_t *length,
                                        uint8_t *wheel_dir);

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
void ppt_trans_pkt_ctrl_report_nack_wheel(uint8_t wheel_dir);
void ppt_trans_pkt_ctrl_report_nack_motion(uint16_t motion_x, uint16_t motion_y);
void ppt_trans_pkt_ctrl_report_send_fail_pkt(void);
void ppt_trans_pkt_ctrl_clear_compensate_motion(void);
bool ppt_trans_pkt_ctrl_get_wheel_is_send_cmpl(void);
static void ppt_trans_pkt_ctrl_cal_compensate_val(volatile int32_t *hist_data,
                                                  uint16_t *optical_data);
static void ppt_trans_pkt_ctrl_update_hist_compensate_val(volatile int32_t *hist_data,
                                                          int16_t optical_data);
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN
/*============================================================================*
 *                              Static Functions
 *============================================================================*/
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/******************************************************************
 * @brief  calculate how many history data could be padded into current pkt data
 * @param  hist_data - accumulated optical offset to compensate
 * @param  optical_data - optical data to send in future packet
 * @retval void
 */
static void ppt_trans_pkt_ctrl_cal_compensate_val(volatile int32_t *hist_data,
                                                  uint16_t *optical_data)
{
    int32_t temp_hist_data = *hist_data;
    int16_t  temp_optical_data = (int16_t)(*optical_data);
    uint8_t optical_data_size = (T_PPT_TRANS_MOTION_DATA_SIZE)ppt_trans_pkt_algo_get_motion_size(
                                    *optical_data);
    int32_t optical_data_upper, optical_data_lower;
    int32_t compensate = 0;

    if (optical_data_size == PPT_TRANS_MOTION_SIZE_0BITS)
    {
        optical_data_size = PPT_PKT_COMPENSATE_RESOLUTION;
    }
    optical_data_size = (1 << optical_data_size);

    if (temp_hist_data > 0)
    {
        optical_data_upper = (1 << (optical_data_size - 1)) - 1;
        optical_data_upper = (optical_data_upper > ppt_trans_pkt_ctrl_compensate_upper) ?
                             optical_data_upper : ppt_trans_pkt_ctrl_compensate_upper;
        compensate = optical_data_upper - temp_optical_data;
        if (compensate > temp_hist_data)
        {
            compensate = temp_hist_data;
        }
    }
    else
    {
        optical_data_lower = (-1) * (1 << (optical_data_size - 1));
        optical_data_lower = (optical_data_lower < ppt_trans_pkt_ctrl_compensate_lower) ?
                             optical_data_lower : ppt_trans_pkt_ctrl_compensate_lower;

        compensate = optical_data_lower - temp_optical_data;
        if (compensate < temp_hist_data)
        {
            compensate = temp_hist_data;
        }
    }
    temp_hist_data -= compensate;
    temp_optical_data += compensate;

    *hist_data = temp_hist_data;
    *optical_data = (uint16_t)temp_optical_data;
    return;
}

/******************************************************************
 * @brief  update history data value
 * @param  hist_data - accumulated optical offset
 * @param  optical_data - optical data that reported failed
 * @retval void
 *
 * @note   caller need to avoid race condition by itself
 */
static void ppt_trans_pkt_ctrl_update_hist_compensate_val(volatile int32_t *hist_data,
                                                          int16_t optical_data)
{
    int32_t temp_hist_data = *hist_data;
    uint8_t sign_bit = (temp_hist_data >= 0) ? 1 : 0;

    temp_hist_data += optical_data;

    if ((sign_bit == 1) && (optical_data > 0))
    {
        *hist_data = (temp_hist_data > 0) ? temp_hist_data : INT32_MAX;
    }
    else if ((sign_bit == 0) && (optical_data < 0))
    {
        *hist_data = (temp_hist_data < 0) ? temp_hist_data : INT32_MIN;
    }
    else
    {
        *hist_data = temp_hist_data;
    }
    return;
}
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  Receive mouse data send request from application layer
 * @param  mouse_data - mouse data to store
 * @param  is_dummy - mouse data is dummy or real data
 * @retval void
 */
void ppt_trans_pkt_ctrl_receive_data(T_PPT_TRANS_MOUSE_DATA mouse_data, bool is_dummy)
{
    uint32_t s = os_lock();
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
    bool compensated = false;
#if PPT_TRANS_PKT_CTRL_DBG_LOG_EN
    uint8_t prev_wheel;
    int16_t prev_x, prev_y;
    int32_t prev_comp_x, prev_comp_y;
    prev_x = mouse_data.optical_x;
    prev_y = mouse_data.optical_y;
    prev_wheel = mouse_data.wheel_direction;
    prev_comp_x = ppt_trans_pkt_ctrl_compensate_x;
    prev_comp_y = ppt_trans_pkt_ctrl_compensate_y;
#endif // PPT_TRANS_PKT_CTRL_DBG_LOG_EN
    if ((mouse_data.wheel_direction == PPT_TRANS_WHEEL_RELEASE_DEF) &&
        (ppt_trans_pkt_ctrl_hist_wheel_read_idx != ppt_trans_pkt_ctrl_hist_wheel_write_idx))
    {
        mouse_data.wheel_direction =
            ppt_trans_pkt_ctrl_compensate_wheel[ppt_trans_pkt_ctrl_hist_wheel_read_idx];
        ppt_trans_pkt_ctrl_hist_wheel_read_idx = (ppt_trans_pkt_ctrl_hist_wheel_read_idx + 1) %
                                                 PPT_PKT_HIST_WHEEL_ARR_SIZE;
        compensated = true;
    }
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_recv_position_vector(mouse_data.optical_x,
                                            mouse_data.optical_y);
#else
    if (ppt_trans_pkt_ctrl_compensate_x != 0)
    {
        compensated = true;
        ppt_trans_pkt_ctrl_cal_compensate_val(&ppt_trans_pkt_ctrl_compensate_x,
                                              (uint16_t *) & (mouse_data.optical_x));
    }

    if (ppt_trans_pkt_ctrl_compensate_y != 0)
    {
        compensated = true;
        ppt_trans_pkt_ctrl_cal_compensate_val(&ppt_trans_pkt_ctrl_compensate_y,
                                              (uint16_t *) & (mouse_data.optical_y));
    }
#endif // PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#if PPT_TRANS_PKT_CTRL_DBG_LOG_EN
    if (compensated)
    {
        APP_PRINT_INFO5("[ppt_trans_pkt_ctrl_receive_data] before compensate x: 0x%x, y: 0x%x, wheel: %d, hist_x: %d, hist_y: %d",
                        prev_x, prev_y, prev_wheel, prev_comp_x, prev_comp_y);
        APP_PRINT_INFO6("[ppt_trans_pkt_ctrl_receive_data] after compensate x: 0x%x, y: 0x%x, wheel: %d, hist_x: %d, hist_y: %d, write_idx: %d",
                        mouse_data.optical_x, mouse_data.optical_y, mouse_data.wheel_direction,
                        ppt_trans_pkt_ctrl_compensate_x, ppt_trans_pkt_ctrl_compensate_y, ppt_trans_pkt_ctrl_write_idx);
    }
#endif // PPT_TRANS_PKT_CTRL_DBG_LOG_EN
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN
    if (((ppt_trans_pkt_ctrl_write_idx + 1) % PPT_PKT_HIST_PKT_SIZE) == ppt_trans_pkt_ctrl_read_idx)
    {
        APP_PRINT_INFO2("[ppt_trans_pkt_ctrl_receive_data] Buffer Overflow!!! read: %d, write: %d, drop oldest data as send fail",
                        ppt_trans_pkt_ctrl_read_idx, ppt_trans_pkt_ctrl_write_idx);
        ppt_trans_pkt_ctrl_read_idx = ((ppt_trans_pkt_ctrl_read_idx + 1) % PPT_PKT_HIST_PKT_SIZE);
        ppt_trans_pkt_ctrl_report_send_fail_pkt();
    }
    if (is_dummy)
    {
        mouse_data.button = ppt_trans_pkt_ctrl_button_mask;
    }
    else
    {
        ppt_trans_pkt_ctrl_button_mask = mouse_data.button;
    }
    memcpy(ppt_trans_pkt_ctrl_hist_list + ppt_trans_pkt_ctrl_write_idx, &mouse_data,
           sizeof(T_PPT_TRANS_MOUSE_DATA));
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_record_pos(ppt_trans_pkt_ctrl_write_idx);
#endif
    ppt_trans_pkt_ctrl_write_idx = ((ppt_trans_pkt_ctrl_write_idx + 1) % PPT_PKT_HIST_PKT_SIZE);
    os_unlock(s);
    return;
}

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
/******************************************************************
 * @brief  Clear packet control history array
 * @retval void
 */
void ppt_trans_pkt_ctrl_clear_history(void)
{
    memset(ppt_trans_pkt_ctrl_hist_list, 0, sizeof(ppt_trans_pkt_ctrl_hist_list));
}

/******************************************************************
 * @brief   get the sequence number of packet
 * @return  sequence number of packet
 */
uint8_t ppt_trans_pkt_ctrl_get_cur_seq(void)
{
    return (uint8_t)ppt_trans_pkt_ctrl_seq_cnt;
}

/******************************************************************
 * @brief  Get current packet control module uint32_t packet sequence
 * @return  uint32_t sequence number of packet
 */
uint32_t ppt_trans_pkt_ctrl_get_u32_seq(void)
{
    return (uint32_t) ppt_trans_pkt_ctrl_u32_seq_cnt;
}
#endif
/******************************************************************
 * @brief  handle sending mouse position compensate data
 * @param  next_pkt - pointer that needs to copy packet data in
 * @param  length - how many bytes are copied in next_pkt
 * @param  wheel_dir - the direction of wheel sent in next_pkt
 * @retval pointer to the packet data to be transmitted
 */
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
void  ppt_trans_pkt_ctrl_get_compensate_data(uint8_t *next_pkt, uint8_t *length, uint8_t *wheel_dir)
{
    if (ppt_trans_pkt_ctrl_read_idx == ppt_trans_pkt_ctrl_write_idx)
    {
        if (false == ppt_trans_handle_transport_status_get())
        {
            T_PPT_TRANS_PKT_HEADER header =
            {
                .d8 = 0
            };
            header.seq_en = PPT_TRANS_FIELD_ENABLE;
            header.pkt_type = PPT_TRANS_PKT_TYPE_OFFSET;
            next_pkt[0] = header.d8;
            next_pkt[1] = (ppt_trans_pkt_ctrl_seq_cnt << 2);
            ppt_trans_pkt_ctrl_seq_cnt = (ppt_trans_pkt_ctrl_seq_cnt + 1) % PPT_PKT_PAYLOAD_SEQ_NUM_SIZE;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
            ppt_trans_pkt_ctrl_u32_seq_cnt++;
#endif
            *length = 2;
        }
        else
        {
            *length = 0;
        }
        return;
    }
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    else
    {
        uint8_t pos_read_idx = (uint8_t)(ppt_trans_pkt_ctrl_read_idx) % PPT_PKT_HIST_PKT_SIZE;
        T_PPT_TRANS_POS_VECTOR_SUM pos = ppt_trans_pos_ctrl_get_hist_pos(pos_read_idx);
        uint8_t temp[6];
        uint8_t temp_size = 4;
        if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV) == false)
        {
            int16_t pos_x = (int16_t)pos.pos_x, pos_y = (int16_t)pos.pos_y;
            temp[0] = pos_x;
            temp[1] = pos_x >> 8;
            temp[2] = pos_y;
            temp[3] = pos_y >> 8;
            temp_size = 4;
        }
        else
        {
            int32_t pos_x = (int32_t)pos.pos_x, pos_y = (int32_t)pos.pos_y;
            temp[0] = pos_x;
            temp[1] = pos_x >> 8;
            temp[2] = pos_x >> 16;
            temp[3] = pos_y;
            temp[4] = pos_y >> 8;
            temp[5] = pos_y >> 16;
            temp_size = 6;
        }
        uint8_t motion_points = PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1;
        uint8_t temp_read_idx = (uint8_t)(ppt_trans_pkt_ctrl_read_idx - (motion_points - 1)) %
                                PPT_PKT_HIST_PKT_SIZE;
        T_PPT_TRANS_MOUSE_DATA temp_packet_src[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
        for (uint8_t i = 0; i < motion_points; i++)
        {
            memcpy(temp_packet_src + i,
                   &(ppt_trans_pkt_ctrl_hist_list[(temp_read_idx + i) % PPT_PKT_HIST_PKT_SIZE]),
                   sizeof(T_PPT_TRANS_MOUSE_DATA));
        }

        ppt_trans_handle_cfg_mgr.ppt_trans_algo_compose_pos_handler(temp_packet_src, motion_points,
                                                                    (uint8_t *)&temp, temp_size, next_pkt,
                                                                    length, ppt_trans_pkt_ctrl_seq_cnt);
        if (ppt_trans_handle_cfg_mgr.report_rate == PPT_REPORT_RATE_LEVEL_8K)
        {
            ppt_trans_pkt_ctrl_report_nack_wheel(temp_packet_src[motion_points - 1].wheel_direction);
            *wheel_dir = PPT_TRANS_WHEEL_RELEASE_DEF;
        }
        else
        {
            *wheel_dir = temp_packet_src[motion_points - 1].wheel_direction;
        }
#if PPT_TRANS_PKT_CTRL_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_pkt_ctrl_get_compensate_data] len: %d, packed packet: [%b]",
                        *length, TRACE_BINARY(*length, next_pkt));
#endif
        ppt_trans_pkt_ctrl_read_idx = ((uint8_t)(ppt_trans_pkt_ctrl_read_idx + 1) % PPT_PKT_HIST_PKT_SIZE);
    }
#endif

    T_PPT_TRANS_PKT_HEADER header =
    {
        .d8 = 0
    };
    header.d8 = next_pkt[0];
    header.pkt_type = PPT_TRANS_PKT_TYPE_COMPENSATE;
    next_pkt[0] = header.d8;
    ppt_trans_pkt_ctrl_seq_cnt = (ppt_trans_pkt_ctrl_seq_cnt + 1) % PPT_PKT_PAYLOAD_SEQ_NUM_SIZE;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pkt_ctrl_u32_seq_cnt++;
#endif
    return;
}
#endif

void  ppt_trans_pkt_ctrl_get_offset_data(uint8_t *next_pkt, uint8_t *length, uint8_t *wheel_dir)
{
    if (ppt_trans_pkt_ctrl_read_idx == ppt_trans_pkt_ctrl_write_idx)
    {
        if (false == ppt_trans_handle_transport_status_get())
        {
            // transport layer is not idle but no sensor data to send
            T_PPT_TRANS_PKT_HEADER header =
            {
                .d8 = 0
            };
            header.seq_en = PPT_TRANS_FIELD_ENABLE;
            header.pkt_type = PPT_TRANS_PKT_TYPE_OFFSET;
            next_pkt[0] = header.d8;
            next_pkt[1] = (ppt_trans_pkt_ctrl_seq_cnt << 2);
            ppt_trans_pkt_ctrl_seq_cnt = (ppt_trans_pkt_ctrl_seq_cnt + 1) % PPT_PKT_PAYLOAD_SEQ_NUM_SIZE;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
            ppt_trans_pkt_ctrl_u32_seq_cnt++;
#endif
            *length = 2;
        }
        else
        {
            *length = 0;
        }
        return;
    }
    else
    {
        uint8_t temp_read_idx = (uint8_t)(ppt_trans_pkt_ctrl_read_idx - (PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1))
                                %
                                PPT_PKT_HIST_PKT_SIZE;
        T_PPT_TRANS_MOUSE_DATA temp_packet_src[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
        for (uint8_t i = 0; i < PPT_PKT_PAYLOAD_DATA_NUM_MAX; i++)
        {
            memcpy(temp_packet_src + i, &(ppt_trans_pkt_ctrl_hist_list[(temp_read_idx + i) %
                                                                                           PPT_PKT_HIST_PKT_SIZE]), sizeof(T_PPT_TRANS_MOUSE_DATA));
            *wheel_dir = temp_packet_src[i].wheel_direction;
        }
        ppt_trans_pkt_algo_compose_pkt(temp_packet_src, PPT_PKT_PAYLOAD_DATA_NUM_MAX, next_pkt, length,
                                       ppt_trans_pkt_ctrl_seq_cnt);
#if PPT_TRANS_PKT_CTRL_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_pkt_ctrl_get_offset_data] len: %d, packed packet: [%b]",
                        *length, TRACE_BINARY(*length, next_pkt));
#endif
        ppt_trans_pkt_ctrl_read_idx = ((uint8_t)(ppt_trans_pkt_ctrl_read_idx + 1) % PPT_PKT_HIST_PKT_SIZE);
    }
    T_PPT_TRANS_PKT_HEADER header =
    {
        .d8 = 0
    };
    header.d8 = next_pkt[0];
    header.pkt_type = PPT_TRANS_PKT_TYPE_OFFSET;

    next_pkt[0] = header.d8;
    ppt_trans_pkt_ctrl_seq_cnt = (ppt_trans_pkt_ctrl_seq_cnt + 1) % PPT_PKT_PAYLOAD_SEQ_NUM_SIZE;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pkt_ctrl_u32_seq_cnt++;
#endif
    return;
}

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/******************************************************************
 * @brief  handle NACK packet wheel compensation
 * @param  wheel_dir - QDEC direction data
 *
 * @note   handle critical section outside this function
 */
void ppt_trans_pkt_ctrl_report_nack_wheel(uint8_t wheel_dir)
{
    uint32_t s = os_lock();
    if (wheel_dir != PPT_TRANS_WHEEL_RELEASE_DEF)
    {
        // APP_PRINT_INFO2("[ppt_trans_pkt_ctrl_report_nack_wheel] compensate wheel dir %d at write idx %d",
        //                 wheel_dir, ppt_trans_pkt_ctrl_hist_wheel_write_idx);
        if ((ppt_trans_pkt_ctrl_hist_wheel_write_idx + 1) % PPT_PKT_HIST_WHEEL_ARR_SIZE ==
            ppt_trans_pkt_ctrl_hist_wheel_read_idx)
        {
            // APP_PRINT_INFO2("[ppt_trans_pkt_ctrl_report_nack_wheel] wheel arr overflow, read idx: %d, write idx: %d",
            //                 ppt_trans_pkt_ctrl_hist_wheel_read_idx, ppt_trans_pkt_ctrl_hist_wheel_write_idx);
        }
        else
        {
            ppt_trans_pkt_ctrl_compensate_wheel[ppt_trans_pkt_ctrl_hist_wheel_write_idx] = wheel_dir;
            ppt_trans_pkt_ctrl_hist_wheel_write_idx = (ppt_trans_pkt_ctrl_hist_wheel_write_idx + 1) %
                                                      PPT_PKT_HIST_WHEEL_ARR_SIZE;
        }
    }
    os_unlock(s);
}

/******************************************************************
 * @brief  check whether wheel queue is empty.
 * @return the check result of wheel queue.
 * @retval bool
 */
bool ppt_trans_pkt_ctrl_get_wheel_is_send_cmpl(void)
{
    return ppt_trans_pkt_ctrl_hist_wheel_read_idx == ppt_trans_pkt_ctrl_hist_wheel_write_idx;
}

/******************************************************************
 * @brief  handle NACK packet motion compensation
 * @param  motion_x - optical x-axis offset data
 * @param  motion_y - optical y-axis offset data
 */
void ppt_trans_pkt_ctrl_report_nack_motion(uint16_t motion_x, uint16_t motion_y)
{
    uint32_t s = os_lock();
    if (motion_x != 0)
    {
        ppt_trans_pkt_ctrl_update_hist_compensate_val(&ppt_trans_pkt_ctrl_compensate_x,
                                                      (int16_t)motion_x);
    }
    if (motion_y != 0)
    {
        ppt_trans_pkt_ctrl_update_hist_compensate_val(&ppt_trans_pkt_ctrl_compensate_y,
                                                      (int16_t)motion_y);
    }
    int32_t temp_compensate_x, temp_compensate_y;
    temp_compensate_x = ppt_trans_pkt_ctrl_compensate_x;
    temp_compensate_y = ppt_trans_pkt_ctrl_compensate_y;
    os_unlock(s);

    // APP_PRINT_INFO4("[ppt_trans_pkt_ctrl_report_nack_motion] x: %d y: %d hist_x: %d, hist_y: %d",
    //                (int8_t)motion_x, (int8_t)motion_y, temp_compensate_x, temp_compensate_y);
}

/******************************************************************
 * @brief  handle packet request send fail compensation
 * @param  motion_x - optical x-axis offset data
 * @param  motion_y - optical y-axis offset data
 * @param  wheel_dir - QDEC direction data
 */
void ppt_trans_pkt_ctrl_report_send_fail_pkt(void)
{
    uint32_t s = os_lock();
    uint8_t rollback_read_idx = (uint8_t)(ppt_trans_pkt_ctrl_read_idx - 1) % PPT_PKT_HIST_PKT_SIZE;
    uint8_t wheel_dir;
    wheel_dir = ppt_trans_pkt_ctrl_hist_list[rollback_read_idx].wheel_direction;

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL == 0
    int16_t  motion_x, motion_y;
    motion_x = ppt_trans_pkt_ctrl_hist_list[rollback_read_idx].optical_x;
    motion_y = ppt_trans_pkt_ctrl_hist_list[rollback_read_idx].optical_y;
    if (motion_x != 0)
    {
        ppt_trans_pkt_ctrl_update_hist_compensate_val(&ppt_trans_pkt_ctrl_compensate_x,
                                                      motion_x);
    }
    if (motion_y != 0)
    {
        ppt_trans_pkt_ctrl_update_hist_compensate_val(&ppt_trans_pkt_ctrl_compensate_y,
                                                      motion_y);
    }
    int32_t temp_compensate_x, temp_compensate_y;
    temp_compensate_x = ppt_trans_pkt_ctrl_compensate_x;
    temp_compensate_y = ppt_trans_pkt_ctrl_compensate_y;
    ppt_trans_pkt_ctrl_hist_list[rollback_read_idx].optical_x = 0;
    ppt_trans_pkt_ctrl_hist_list[rollback_read_idx].optical_y = 0;

#endif
    ppt_trans_pkt_ctrl_report_nack_wheel(wheel_dir);
    os_unlock(s);
#if (PPT_TRANS_PKT_CTRL_DBG_LOG_EN && (PPT_TRANS_FEATURE_SUPPORT_POS_CTRL == 0))
    APP_PRINT_INFO5("[ppt_trans_pkt_ctrl_report_send_fail_pkt] opt_x: %d, opt_y: %d, wheel: %d, hist_x: %d, hist_y: %d",
                    motion_x, motion_y, wheel_dir, temp_compensate_x, temp_compensate_y);
#endif
}

/******************************************************************
 * @brief  clear send fail/nack motion compensation
 */
void ppt_trans_pkt_ctrl_clear_compensate_motion(void)
{
    uint32_t s = os_lock();
    ppt_trans_pkt_ctrl_compensate_x = 0;
    ppt_trans_pkt_ctrl_compensate_y = 0;
    os_unlock(s);
}

#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

/******************************************************************
 * @brief  init transport layer packet control & managements
 * @retval void
 */
void ppt_trans_pkt_ctrl_init(void)
{
    ppt_trans_pkt_ctrl_write_idx = 0;
    ppt_trans_pkt_ctrl_read_idx = 0;
    memset(ppt_trans_pkt_ctrl_hist_list, 0, sizeof(ppt_trans_pkt_ctrl_hist_list));
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
    ppt_trans_pkt_ctrl_compensate_x = 0;
    ppt_trans_pkt_ctrl_compensate_y = 0;
    ppt_trans_pkt_ctrl_hist_wheel_read_idx = 0;
    ppt_trans_pkt_ctrl_hist_wheel_write_idx = 0;
#endif
}
