/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_sync_ctrl.c
   * @brief     2.4g stack/sync lib related callback functions
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
#include "utils.h"
#include "ppt_sync.h"
#include "ppt_sync_app.h"
#include "app_section.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_pkt_ctrl.h"
#include "ppt_trans_long_pkt_ctrl.h"
#include "ppt_trans_pkt_algo.h"
#include "ppt_trans_sync_ctrl.h"
#include "ppt_trans_test_handle.h"
#include "ppt_trans_test_stub_sync.h"
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
#include "ppt_trans_chann_ctrl.h"
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#endif

/*============================================================================*
 *                              Declares
 *============================================================================*/
void ppt_trans_sync_ctrl_report_data(void);
static void ppt_trans_sync_ctrl_recv_msg_cb(uint8_t *p_data, uint16_t len,
                                            sync_receive_info_t *info);
static void ppt_trans_sync_ctrl_send_msg_cb(sync_msg_type_t type, uint8_t *p_data, uint16_t len,
                                            sync_send_info_t *info);
static void ppt_trans_sync_ctrl_report_idle_cb(void *p_timer);
static void ppt_trans_sync_ctrl_timer_trigger_report(void *param1, uint32_t param2);


#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
static void ppt_trans_sync_ctrl_update_pkt_mask(uint32_t *bitmask,
                                                uint8_t new_data);
static void ppt_trans_sync_ctrl_enable_rpt_rate_switch_cb(TimerHandle_t p_timer);
static void ppt_trans_sync_ctrl_set_rpt_rate_switch(bool enable);
#endif

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
static void ppt_trans_sync_ctrl_update_fail_pkt(uint8_t *p_data, uint16_t length_src,
                                                T_PPT_TRANS_SYNC_CTRL_ERR_CODE err_code);
static void ppt_trans_sync_ctrl_update_fail_state(bool pkt_failed);
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
static void ppt_trans_sync_ctrl_report_stats(TimerHandle_t p_timer);
#endif

/*============================================================================*
 *                              Variables
 *============================================================================*/
/** app callback for payload handling when receive packet */
static ppt_trans_handle_receive_pkt_cb callback_fp = NULL;
/** app callback for data handling when receive raw data */
static ppt_trans_handle_receive_raw_cb rx_raw_callback_fp = NULL;
void *ppt_trans_sync_ctrl_idle_timer = NULL;

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
/** adaptive report rate cool down timer */
void *ppt_adp_guard_timer = NULL;
/** adaptive report rate enable/disable flag */
static bool ppt_adp_enable_switch_rate = false;
/** statistic of last 96 ~ 65 send result records */
static uint32_t ppt_trans_sync_ctrl_pkt_record_upper = 0;
/** statistic of last 64 ~ 33 send result records */
static uint32_t ppt_trans_sync_ctrl_pkt_record_middle = 0;
/** statistic of last 32 send result records */
static uint32_t ppt_trans_sync_ctrl_pkt_record_lower = 0;
/** statistic of failed packets in last 96 send records */
static uint8_t  ppt_trans_sync_ctrl_pkt_fail_cnt = 0;
#endif

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/** state machine for packet re-send handling */
static T_PPT_TRANS_SYNC_NACK_STATE ppt_trans_sync_ctrl_nack_state = PPT_TRANS_SYNC_CTRL_NACK_ZERO;
/** packet buffer for packet re-send handling */
static T_PPT_TRANS_SYNC_CTRL_FAILED_PKT
ppt_trans_sync_ctrl_nacked_data_list[PPT_FAIL_PKT_LIST_SIZE] = {0};
/** read index of packet buffer for packet re-send handling */
static uint8_t  ppt_trans_sync_ctrl_nacked_data_read_idx = 0;
/** write index of packet buffer for packet re-send handling */
static uint8_t  ppt_trans_sync_ctrl_nacked_data_write_idx = 0;
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
void *ppt_trans_sync_ctrl_report_stat_timer = NULL;
/** How many nack packets between each report interval */
static uint32_t ppt_trans_sync_ctrl_stat_nack_cnt = 0;
/** How many ack packets between each report interval */
static uint32_t ppt_trans_sync_ctrl_stat_ack_cnt = 0;
/** How many send fail packets between each report interval */
static uint32_t ppt_trans_sync_ctrl_stat_send_fail_cnt = 0;
/** How many packets received between each report interval */
static uint32_t ppt_trans_sync_ctrl_stat_recv_cnt = 0;
/** How many invalid packets received between each report interval */
static uint32_t ppt_trans_sync_ctrl_stat_recv_inval_cnt = 0;
/** previous report tick in ms */
static uint64_t ppt_trans_sync_ctrl_stat_timestamp = 0;
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
/** The previous send success sequence of position packet */
static uint8_t prev_ankor_send_seq = 0xFF;
#endif

/*============================================================================*
 *                              Static Functions
 *============================================================================*/
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
/******************************************************************
 * @brief  adaptive report related, enable/disable switching report rate
 * @param  enable - enable or disable switching
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_set_rpt_rate_switch(bool enable)
{
    uint32_t s = os_lock();
    ppt_adp_enable_switch_rate = enable;
    os_unlock(s);
}

/******************************************************************
 * @brief  Timer callback for switching report rate
 * @param  p_timer - not used
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_enable_rpt_rate_switch_cb(TimerHandle_t p_timer)
{
    ppt_trans_sync_ctrl_set_rpt_rate_switch(true);
    APP_PRINT_INFO0("[ppt_trans_sync_ctrl_enable_rpt_rate_switch_cb] enable report rate switch");
}

/******************************************************************
 * @brief  update history packet event mask
 * @param  bitmask - bitmask to be updated
 * @param  new_data - LSB new data to be recorded
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_update_pkt_mask(uint32_t *bitmask, uint8_t new_data)
{
    (*bitmask) <<= 1;
    if (new_data)
    {
        (*bitmask) |= 0x00000001;
    }
    else
    {
        (*bitmask) &= (~0x00000001);
    }
    return;
}
#endif // PPT_TRANS_FEATURE_ADP_RPT_RATE_EN

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
/******************************************************************
 * @brief  Timer callback for monitor packet transmission status
 * @param  p_timer - not used
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_report_stats(TimerHandle_t p_timer)
{
    uint32_t s = os_lock();
    uint64_t temp_time = os_sys_time_get();
    uint64_t time_diff = (temp_time > ppt_trans_sync_ctrl_stat_timestamp) ?
                         (temp_time - ppt_trans_sync_ctrl_stat_timestamp) : (INT64_MAX - ppt_trans_sync_ctrl_stat_timestamp +
                                                                             temp_time);
    ppt_trans_sync_ctrl_stat_timestamp = temp_time;
    uint32_t temp_total_cnt = ppt_trans_sync_ctrl_stat_ack_cnt + ppt_trans_sync_ctrl_stat_nack_cnt +
                              ppt_trans_sync_ctrl_stat_send_fail_cnt;
    uint32_t temp_ack_cnt = ppt_trans_sync_ctrl_stat_ack_cnt;
    uint32_t temp_nack_cnt =  ppt_trans_sync_ctrl_stat_nack_cnt;
    uint32_t temp_send_fail_cnt = ppt_trans_sync_ctrl_stat_send_fail_cnt;
    uint32_t report_rate = (uint32_t)(temp_total_cnt * 1000 / (uint32_t)time_diff);
    ppt_trans_sync_ctrl_stat_ack_cnt = 0;
    ppt_trans_sync_ctrl_stat_nack_cnt = 0;
    ppt_trans_sync_ctrl_stat_send_fail_cnt = 0;

    uint32_t temp_recv_valid_cnt = ppt_trans_sync_ctrl_stat_recv_cnt;
    uint32_t temp_recv_invalid_cnt = ppt_trans_sync_ctrl_stat_recv_inval_cnt;
    ppt_trans_sync_ctrl_stat_recv_cnt = 0;
    ppt_trans_sync_ctrl_stat_recv_inval_cnt = 0;
    os_unlock(s);
    APP_PRINT_INFO5("[ppt_trans_sync_ctrl_report_stats] Total PKT Sent: %d, ACK: %d, NACK: %d, Send Fail: %d, Report Rate: %d",
                    temp_total_cnt, temp_ack_cnt, temp_nack_cnt, temp_send_fail_cnt, report_rate);
    APP_PRINT_INFO3("[ppt_trans_sync_ctrl_report_stats] Total PKT Recv: %d, Valid: %d, Invalid: %d",
                    temp_recv_valid_cnt + temp_recv_invalid_cnt, temp_recv_valid_cnt, temp_recv_invalid_cnt);
}

#endif

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/******************************************************************
 * @brief  handle packet sending fail compensation
 * @param  p_data - pointer of the failed packet data.
 * @param  len - data length in bytes.
 * @param  err_code - error code of the packet fail @ref T_PPT_TRANS_SYNC_CTRL_ERR_CODE
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_update_fail_pkt(uint8_t *p_data, uint16_t length_src,
                                                T_PPT_TRANS_SYNC_CTRL_ERR_CODE err_code)
{
    uint32_t s = os_lock();
    T_PPT_TRANS_MOUSE_DATA mouse_data;
    ppt_trans_pkt_algo_get_latest_motion(p_data, length_src, &mouse_data);
    ppt_trans_sync_ctrl_nacked_data_list[ppt_trans_sync_ctrl_nacked_data_write_idx].fail_reason =
        err_code;
    ppt_trans_sync_ctrl_nacked_data_list[ppt_trans_sync_ctrl_nacked_data_write_idx].optical_x =
        mouse_data.optical_x;
    ppt_trans_sync_ctrl_nacked_data_list[ppt_trans_sync_ctrl_nacked_data_write_idx].optical_y =
        mouse_data.optical_y;
    ppt_trans_pkt_ctrl_report_nack_wheel(mouse_data.wheel_direction);
    ppt_trans_sync_ctrl_nacked_data_write_idx = (ppt_trans_sync_ctrl_nacked_data_write_idx + 1) %
                                                PPT_FAIL_PKT_LIST_SIZE;
    os_unlock(s);
    return;
}

/******************************************************************
 * @brief  update packet send fail state machine
 * @param  pkt_failed - packet transmission failed or not
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_update_fail_state(bool pkt_failed)
{
    uint32_t s = os_lock();
    if (pkt_failed == true)
    {
        if (ppt_trans_sync_ctrl_nack_state < PPT_TRANS_SYNC_CTRL_NACK_THREE)
        {
            ppt_trans_sync_ctrl_nack_state = (T_PPT_TRANS_SYNC_NACK_STATE)(ppt_trans_sync_ctrl_nack_state + 1);
        }
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_INFO4("[ppt_trans_sync_ctrl_update_fail_state] result: %d, fail rd idx: %d, reason: %d, state: %d",
                        pkt_failed, ppt_trans_sync_ctrl_nacked_data_read_idx,
                        ppt_trans_sync_ctrl_nacked_data_list[ppt_trans_sync_ctrl_nacked_data_read_idx].fail_reason,
                        ppt_trans_sync_ctrl_nack_state);
#endif
    }
    else
    {
        ppt_trans_sync_ctrl_nacked_data_read_idx = ppt_trans_sync_ctrl_nacked_data_write_idx;
        ppt_trans_sync_ctrl_nack_state = PPT_TRANS_SYNC_CTRL_NACK_ZERO;
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_sync_ctrl_update_fail_state] result: %d, state: %d",
                        pkt_failed, ppt_trans_sync_ctrl_nack_state);
#endif
    }

    if (ppt_trans_sync_ctrl_nack_state == PPT_TRANS_SYNC_CTRL_NACK_THREE)
    {
        T_PPT_TRANS_SYNC_CTRL_FAILED_PKT compensate_pkt =
            ppt_trans_sync_ctrl_nacked_data_list[ppt_trans_sync_ctrl_nacked_data_read_idx];
        ppt_trans_sync_ctrl_nacked_data_read_idx = (ppt_trans_sync_ctrl_nacked_data_read_idx + 1) %
                                                   PPT_FAIL_PKT_LIST_SIZE;
        ppt_trans_pkt_ctrl_report_nack_motion(compensate_pkt.optical_x, compensate_pkt.optical_y);
    }
    os_unlock(s);
}

#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

/******************************************************************
 * @brief  sync report idle status callback
 * @retval void
 */
static void ppt_trans_sync_ctrl_report_idle_cb(void *p_timer)
{
    // ppt_trans_handle_notify_continuous_send(false);
    if (!ppt_trans_handle_transport_status_get())
    {
        os_timer_pend_function_call(ppt_trans_sync_ctrl_timer_trigger_report, NULL, 0);
    }
}

/******************************************************************
 * @brief  ppt_trans_sync_ctrl_recv_msg_cb
 * @param  p_data - pointer to receive data.
 * @param  len - data length.
 * @param  info - receive info recorded by sync layer
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_recv_msg_cb(uint8_t *p_data, uint16_t len,
                                            sync_receive_info_t *info)
{
    if (rx_raw_callback_fp != NULL)
    {
        if (rx_raw_callback_fp(p_data, len, info) == true)
        {
            return;
        }
    }

    if ((len < ((PPT_PKT_PAYLOAD_SIZE_HEADER + PPT_PKT_PAYLOAD_SIZE_SEQ + 7) / 8)) ||
        (len > ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_recv))
    {
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_ERROR2("[ppt_trans_sync_ctrl_recv_msg_cb] illegal packet, len: %d, data: [%b]",
                         len, TRACE_BINARY(len, p_data));
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
        ppt_trans_sync_ctrl_stat_recv_inval_cnt += 1;
#endif
        return;
    }

    PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_RX_RECV);
    T_PPT_TRANS_MOUSE_DATA data_out[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
    T_PPT_TRANS_PKT_HEADER header;
    header.d8 = p_data[0];
    if (header.seq_en != PPT_TRANS_FIELD_ENABLE)
    {
#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
        ppt_trans_sync_ctrl_stat_recv_inval_cnt += 1;
#endif

#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_ERROR1("[ppt_trans_sync_ctrl_recv_msg_cb] header invaild: 0x%x", header.d8);
#endif
        PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_RX_RECV);
        return;
    }

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
    ppt_trans_sync_ctrl_stat_recv_cnt += 1;
#endif

#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_sync_ctrl_recv_msg_cb] length: %d, data: [%b]",
                    len, TRACE_BINARY(len, p_data));
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_handle_recv_msg(p_data, len, info);
    PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_RX_RECV);
    return;
#else

    uint8_t length;
    uint8_t seq_num;
    uint8_t used_bytes = 0;
    PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_RX_PARSE);
    ppt_trans_pkt_algo_parse_pkt(p_data, len, data_out, &length, &seq_num, &used_bytes);

    if (header.long_pkt_en == PPT_TRANS_FIELD_ENABLE &&
        header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
    {
        ppt_trans_long_pkt_parse_pkt(p_data + used_bytes, len - used_bytes, *info);
    }

    PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_RX_PARSE);
    if ((callback_fp != NULL) && (length > 0))
    {
        callback_fp(data_out, length, seq_num);
    }
#endif
    PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_RX_RECV);
}

/**
 * @brief callback from timer queue to trigger sending left data
 *
 * @param param1
 * @param param2
 */
static void ppt_trans_sync_ctrl_timer_trigger_report(void *param1, uint32_t param2)
{
    if (false == ppt_trans_handle_get_sensor_status())
    {
        T_PPT_TRANS_MOUSE_DATA dummy_data = {0};
        ppt_trans_pkt_ctrl_receive_data(dummy_data, true);
        ppt_trans_sync_ctrl_report_data();
    }
}

/******************************************************************
 * @brief  ppt_trans_sync_ctrl_send_msg_cb
 * @param  type - which sync mechanism to send packet.
 * @param  p_data - packet data sent.
 * @param  len - packet data length
 * @param  info - send result
 * @return none
 * @retval void
 */
static void ppt_trans_sync_ctrl_send_msg_cb(sync_msg_type_t type, uint8_t *p_data, uint16_t len,
                                            sync_send_info_t *info)
{
    PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_TX_CB);
    uint8_t seq_num = ppt_trans_pkt_algo_get_seq_num(p_data, len);
    uint8_t used_bytes = ppt_trans_pkt_algo_calc_pkt_len(p_data, len);
    T_PPT_TRANS_PKT_HEADER header =
    {
        .d8 = p_data[0]
    };
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
    APP_PRINT_INFO4("[ppt_trans_sync_ctrl_send_msg_cb] result: %d, len: %d, data: [%b], seq_num: %d",
                    info->res, len, TRACE_BINARY(len, p_data), seq_num);
#endif
    if (info->res != SYNC_SEND_RESULT_ACKED)
    {
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_sync_ctrl_send_msg_cb] seq %d NACKed, data: [%b]", seq_num,
                        TRACE_BINARY(len, p_data));
#endif

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
        ppt_trans_sync_ctrl_pkt_fail_cnt++;
#endif

        PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_TX_COMPENSATE);
        if (false == (header.seq_en == PPT_TRANS_FIELD_ENABLE &&
                      header.pkt_type == PPT_TRANS_PKT_TYPE_COMPENSATE))
        {
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
            ppt_trans_sync_ctrl_update_fail_pkt(p_data, len,
                                                info->res + PPT_TRANS_SYNC_CTRL_ERR_CODE_SEND_RESULT_START);
            ppt_trans_sync_ctrl_update_fail_state(true);
#endif
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
            if (header.long_pkt_en == PPT_TRANS_FIELD_ENABLE &&
                header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
            {
                ppt_trans_long_pkt_handle_send_fail(p_data + used_bytes);
            }
#endif
        }
        PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_TX_COMPENSATE);

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
        if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
        {
            uint8_t seq = p_data[1] >> 2; // seqence maximun is 63
            ppt_trans_pos_ctrl_handle_nack(seq, header.pkt_type == PPT_TRANS_PKT_TYPE_COMPENSATE);
        }
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
        ppt_trans_sync_ctrl_stat_nack_cnt += 1;
#endif
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
        ppt_trans_chann_ctrl_nack_cnt_inc();
#endif
    }
    else
    {
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_INFO1("[ppt_trans_sync_ctrl_send_msg_cb] seq %d ACKed", seq_num);
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
        if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
        {
            uint8_t seq = p_data[1] >> 2; // seqence maximun is 63
            ppt_trans_pos_ctrl_handle_ack(seq, header.pkt_type == PPT_TRANS_PKT_TYPE_COMPENSATE);
        }
#endif
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
        ppt_trans_sync_ctrl_update_fail_state(false);
#endif
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
        if (header.long_pkt_en == PPT_TRANS_FIELD_ENABLE &&
            header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
        {
            ppt_trans_long_pkt_recv_ack(p_data + used_bytes, *info);
        }
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
        ppt_trans_sync_ctrl_stat_ack_cnt += 1;
#endif
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
        ppt_trans_chann_ctrl_ack_cnt_inc();
#endif
    }

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    if ((ppt_trans_sync_ctrl_pkt_record_upper & 0x80000000) && (ppt_trans_sync_ctrl_pkt_fail_cnt > 0))
    {
        ppt_trans_sync_ctrl_pkt_fail_cnt--;
    }

    ppt_trans_sync_ctrl_update_pkt_mask(&ppt_trans_sync_ctrl_pkt_record_upper,
                                        ((ppt_trans_sync_ctrl_pkt_record_middle & 0x80000000) ? 1 : 0));
    ppt_trans_sync_ctrl_update_pkt_mask(&ppt_trans_sync_ctrl_pkt_record_middle,
                                        ((ppt_trans_sync_ctrl_pkt_record_lower & 0x80000000) ? 1 : 0));
    ppt_trans_sync_ctrl_update_pkt_mask(&ppt_trans_sync_ctrl_pkt_record_lower,
                                        ((info->res != SYNC_SEND_RESULT_ACKED) ? 1 : 0));

    if ((true == ppt_adp_enable_switch_rate) &&
        (true == ppt_trans_handle_transport_status_get()))
    {
        if ((PPT_WATCH_HIST_PKT_CNT - ppt_trans_sync_ctrl_pkt_fail_cnt) >= PPT_INC_RPT_RATE_CNT)
        {
            ppt_trans_sync_ctrl_set_rpt_rate_switch(false);
            ppt_trans_handle_report_rate_change(PPT_TRANS_INC_REPORT_RATE_REQ);
            ppt_trans_sync_ctrl_adp_guard_timer_enable();
        }
        else if (ppt_trans_sync_ctrl_pkt_fail_cnt >= PPT_DEC_RPT_RATE_CNT)
        {
            ppt_trans_sync_ctrl_set_rpt_rate_switch(false);
            ppt_trans_handle_report_rate_change(PPT_TRANS_DEC_REPORT_RATE_REQ);
            ppt_trans_sync_ctrl_adp_guard_timer_enable();
        }
    }
#endif // PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT) == false)
    {
        // handle transport layer disabled before send callback
        if (os_timer_is_timer_active(&ppt_trans_sync_ctrl_idle_timer))
        {
            os_timer_stop(&ppt_trans_sync_ctrl_idle_timer);
        }
    }
    else
    {
        if (false == ppt_trans_handle_get_sensor_status())
        {
            void *dummy_param = NULL;
            if (os_timer_is_timer_active(&ppt_trans_sync_ctrl_idle_timer))
            {
                os_timer_stop(&ppt_trans_sync_ctrl_idle_timer);
            }
            ppt_trans_sync_ctrl_report_idle_cb(dummy_param);
        }
        else
        {
            // os_timer_restart(&ppt_trans_sync_ctrl_idle_timer, PPT_TRANS_SYNC_CTRL_REPORT_IDLE_ITVL);
        }
    }
    PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_TX_CB);
}

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  handle on-air data transmission
 *
 * @retval void
 */
void ppt_trans_sync_ctrl_report_data(void)
{
    /** buffer of next packet to send
     *  since ppt_trans_pkt_algo_compose_pkt will clear next byte before write
     *  therefore will clear out-of-bound byte if this buffer is full.
     *  add one byte padding to avoid it.
    */
    uint32_t s = os_lock();
    uint8_t ppt_trans_sync_ctrl_next_packet[PPT_PKT_1K_M2S_PAYLOAD_SIZE_MAX + 1] = {0};
    /** length of next packet to send */
    uint8_t  ppt_trans_sync_ctrl_next_pkt_len = 0;
    PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_TX_COMPOSE_PKT);

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    static uint8_t pkt_count = 0;
    bool pos_ctrl = false;

    static uint32_t prev_send_tick = 0;
    static uint32_t prev_send_succ_tick = 0;
    extern uint32_t (*platform_rtc_get_counter)(void);
    uint32_t cur = platform_rtc_get_counter();
    uint32_t tick_gap = rtc_clk_to_us(clk_cnt_diff(prev_send_tick, cur, BIT64(32)), ROUND);
    prev_send_tick = cur;
    uint32_t u32_seq = UINT32_MAX;
#define FORCE_POS_CE_GAP 3000
    if (ppt_trans_handle_cfg_mgr.ppt_trans_role == PPT_TRANS_ROLE_MASTER)
    {
        u32_seq = ppt_trans_pkt_ctrl_get_u32_seq();
        if (pkt_count == PPT_TRANS_POS_CTRL_SYNC_INTERVAL ||
            ppt_trans_pos_ctrl_judge_force_pos_packet(ppt_trans_pkt_ctrl_get_cur_seq(), u32_seq) ||
            tick_gap >= FORCE_POS_CE_GAP)
        {
            pos_ctrl = true;
            pkt_count = 0;
        }
        else
        {
            pkt_count++;
        }
    }
    else
    {
        pkt_count = 0;
    }

    uint8_t wheel_dir = PPT_TRANS_WHEEL_RELEASE_DEF;

    if (pos_ctrl)
    {
        ppt_trans_pkt_ctrl_get_compensate_data(ppt_trans_sync_ctrl_next_packet,
                                               &ppt_trans_sync_ctrl_next_pkt_len, &wheel_dir);
    }
    else
    {
        ppt_trans_pkt_ctrl_get_offset_data(ppt_trans_sync_ctrl_next_packet,
                                           &ppt_trans_sync_ctrl_next_pkt_len, &wheel_dir);
    }
#else
    uint8_t wheel_dir = PPT_TRANS_WHEEL_RELEASE_DEF;
    ppt_trans_pkt_ctrl_get_offset_data(ppt_trans_sync_ctrl_next_packet,
                                       &ppt_trans_sync_ctrl_next_pkt_len, &wheel_dir);
#endif
    if (ppt_trans_sync_ctrl_next_pkt_len == 0)
    {
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
        APP_PRINT_INFO0("[ppt_trans_sync_ctrl_report_data] no next pkt");
#endif
        if (true == ppt_trans_handle_get_sensor_status())
        {
            // ppt_trans_handle_notify_continuous_send(false);
            os_timer_stop(&ppt_trans_sync_ctrl_idle_timer);
        }
    }
    else
    {
        //uint32_t s = os_lock();
        T_PPT_TRANS_PKT_HEADER header =
        {
            .d8 = ppt_trans_sync_ctrl_next_packet[0]
        };

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
        if (header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
        {
            ppt_trans_long_pkt_ctrl_get_data(ppt_trans_sync_ctrl_next_packet,
                                             &ppt_trans_sync_ctrl_next_pkt_len);
        }
#endif

        /** start calculate data from sequence */

        PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_TX_COMPOSE_PKT);
        sync_msg_set_dynamic_retrans(0, 0);
#if PPT_TRANS_TEST_ENABLE
        ppt_trans_test_stub_sync_send(SYNC_MSG_TYPE_DYNAMIC_RETRANS, ppt_trans_sync_ctrl_next_packet,
                                      ppt_trans_sync_ctrl_next_pkt_len,
                                      ppt_trans_sync_ctrl_send_msg_cb);
#else
        sync_err_code_t err;
        if (ppt_trans_sync_ctrl_next_pkt_len > ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent)
        {
            err = SYNC_ERR_CODE_INVALID_LENGTH;
        }
        else
        {
            err = sync_msg_send(SYNC_MSG_TYPE_INFINITE_RETRANS, ppt_trans_sync_ctrl_next_packet,
                                ppt_trans_sync_ctrl_next_pkt_len,
                                ppt_trans_sync_ctrl_send_msg_cb);
        }
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
        if (pos_ctrl && err == SYNC_ERR_CODE_SUCCESS)
        {
            prev_ankor_send_seq =  ppt_trans_sync_ctrl_next_packet[1] >> 2;
            prev_send_succ_tick = cur;
            ppt_trans_pos_ctrl_handle_rec_seq(prev_ankor_send_seq,
                                              u32_seq);
        }
#endif
        //os_unlock(s);

        if (err != SYNC_ERR_CODE_SUCCESS)
        {
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
            if (pos_ctrl)
            {
                pkt_count = PPT_TRANS_POS_CTRL_SYNC_INTERVAL;
            }
#endif
            uint8_t seq_num = ppt_trans_pkt_algo_get_seq_num(ppt_trans_sync_ctrl_next_packet,
                                                             ppt_trans_sync_ctrl_next_pkt_len);
#if PPT_TRANS_SYNC_CTRL_DBG_LOG_EN
            APP_PRINT_ERROR2("[ppt_trans_sync_ctrl_report_data] send data failed! err code: %d, seq: %d",
                             err, seq_num);
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
            ppt_trans_sync_ctrl_stat_send_fail_cnt += 1;
#endif

#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
            ppt_trans_chann_ctrl_send_fail_cnt_inc();
#endif

            if ((err == SYNC_ERR_CODE_FULL_QUEUE) ||
                (err == SYNC_ERR_CODE_INVALID_LENGTH) ||
                (err == SYNC_ERR_CODE_UNKNOWN))
            {
                PPT_TRANS_GPIO_LEVEL_HIGH(TIME_DEBUG_CHAN_TX_COMPENSATE);
                T_PPT_TRANS_PKT_HEADER header =
                {
                    .d8 = ppt_trans_sync_ctrl_next_packet[0]
                };
#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
                if (header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET ||
                    ppt_trans_handle_cfg_mgr.report_rate != PPT_REPORT_RATE_LEVEL_8K)
                {
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
                    ppt_trans_pkt_ctrl_report_nack_wheel(wheel_dir);
#else
                    ppt_trans_pkt_ctrl_report_send_fail_pkt();
#endif
                }
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN
                uint8_t used_bytes = ppt_trans_pkt_algo_calc_pkt_len(ppt_trans_sync_ctrl_next_packet,
                                                                     ppt_trans_sync_ctrl_next_pkt_len);
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
                if (header.long_pkt_en == PPT_TRANS_FIELD_ENABLE &&
                    header.pkt_type == PPT_TRANS_PKT_TYPE_OFFSET)
                {
                    ppt_trans_long_pkt_handle_send_fail(ppt_trans_sync_ctrl_next_packet + used_bytes);
                }
#endif
                PPT_TRANS_GPIO_LEVEL_LOW(TIME_DEBUG_CHAN_TX_COMPENSATE);
                if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT) == false)
                {
                    // handle transport layer disabled before send callback
                    if (os_timer_is_timer_active(&ppt_trans_sync_ctrl_idle_timer))
                    {
                        os_timer_stop(&ppt_trans_sync_ctrl_idle_timer);
                    }
                }
                else
                {
                    if (false == ppt_trans_handle_get_sensor_status())
                    {
                        os_timer_restart(&ppt_trans_sync_ctrl_idle_timer, PPT_TRANS_SYNC_CTRL_REPORT_IDLE_ITVL);
                    }
                }
            }
        }
#endif // PPT_TRANS_TEST_ENABLE
    }
    os_unlock(s);
}

/******************************************************************
 * @brief  initialize adaptive report rate feature parameters
 * @retval void
 */
void ppt_trans_sync_ctrl_init_adp_feature(void)
{
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    ppt_adp_enable_switch_rate = false;
    ppt_trans_sync_ctrl_pkt_record_upper = 0;
    ppt_trans_sync_ctrl_pkt_record_middle = 0;
    ppt_trans_sync_ctrl_pkt_record_lower = 0;
    ppt_trans_sync_ctrl_pkt_fail_cnt = 0;
#else
    APP_PRINT_WARN0("[ppt_trans_sync_ctrl_init_adp_feature] adaptive report rate disabled!");
#endif
}

/******************************************************************
 * @brief register app callback for receiving data
 * @param  cb - callback function when packet received
*/
void ppt_trans_sync_ctrl_reg_receive_cb(ppt_trans_handle_receive_pkt_cb cb)
{
    APP_PRINT_INFO1("[ppt_trans_sync_ctrl_reg_receive_cb] callback is 0x%x", cb);
    callback_fp = cb;
}

/******************************************************************
 * @brief register app callback for receiving raw data
 * @param  cb - callback function when raw data received
*/
void ppt_trans_sync_ctrl_reg_receive_raw_cb(ppt_trans_handle_receive_raw_cb cb)
{
    APP_PRINT_INFO1("[ppt_trans_handle_receive_raw_cb] callback is 0x%x", cb);
    rx_raw_callback_fp = cb;
}

/******************************************************************
 * @brief reset sync layer callback to transport layer
*/
void ppt_trans_sync_ctrl_reset_receive_cb(void)
{
    sync_msg_reg_receive_cb(ppt_trans_sync_ctrl_recv_msg_cb);
}

#if PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID
/**
 * @brief Configure pairing id used for sync lib
 *
 * @param pair_id
 * @return true - configure success
 * @return false - configure failed
 */
bool ppt_trans_sync_ctrl_set_pair_id(uint32_t pair_id)
{
    bool ret = sync_set_pair_id(pair_id);
    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_ERROR1("[ppt_trans_sync_ctrl_set_pair_id] configure pairing id failed, reason: %d",
                         ret);
        return false;
    }
    return true;
}

/**
 * @brief Obtain pairing id used for sync lib
 *
 * @param pair_id
 * @return true - get success
 * @return false - get failed
 */
bool ppt_trans_sync_ctrl_get_pair_id(uint32_t *pair_id)
{
    bool ret = sync_get_pair_id(pair_id);
    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_ERROR1("[ppt_trans_sync_ctrl_get_pair_id] obtain pairing id failed, reason: %d", ret);
        return false;
    }
    return true;
}
#endif

/******************************************************************
 * @brief  sync control module init
 * @return none
 * @retval void
 */
void ppt_trans_sync_ctrl_init(void)
{
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    if (false == os_timer_create(&ppt_adp_guard_timer,
                                 "adp rate guard timer", 0, PPT_ADP_RPT_GUARD_TIME,
                                 false, ppt_trans_sync_ctrl_enable_rpt_rate_switch_cb))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] adp timer creation failed");
    }
#endif

    if (false == os_timer_create(&ppt_trans_sync_ctrl_idle_timer,
                                 "sync ctrl idle timer", 0, PPT_TRANS_SYNC_CTRL_REPORT_IDLE_ITVL,
                                 false, ppt_trans_sync_ctrl_report_idle_cb))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] idle timer creation failed");
    }

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
    if (false == os_timer_create(&ppt_trans_sync_ctrl_report_stat_timer,
                                 "send stat timer", 0, PPT_TRANS_SYNC_CTRL_REPORT_STAT_ITVL,
                                 true, ppt_trans_sync_ctrl_report_stats))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] stat timer creation failed");
    }
    else
    {
        ppt_trans_sync_ctrl_stat_timestamp = os_sys_time_get();
        ppt_trans_sync_ctrl_stat_ack_cnt = 0;
        ppt_trans_sync_ctrl_stat_nack_cnt = 0;
        ppt_trans_sync_ctrl_stat_send_fail_cnt = 0;
        ppt_trans_sync_ctrl_stat_recv_cnt = 0;
        ppt_trans_sync_ctrl_stat_recv_inval_cnt = 0;
        os_timer_restart(&ppt_trans_sync_ctrl_report_stat_timer, PPT_TRANS_SYNC_CTRL_REPORT_STAT_ITVL);
    }
#endif

    if (ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT))
    {
        sync_msg_reg_receive_cb(ppt_trans_sync_ctrl_recv_msg_cb);
    }
#if FEATURE_SUPPORT_PROPRIETARY_HOPPING
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
    sync_channel_ctrl_set_channel_version(SYNC_CHANNEL_USING_STATISTIC);
#elif PPT_TRANS_FEATURE_SUPPORT_CHANNEL_TRAVERSE
    sync_channel_ctrl_set_channel_version(SYNC_CHANNEL_TRAVERSE_WHEN_LOST);
#endif
#endif

#if PPT_TRANS_FEATURE_CONFIG_CHAN
    uint16_t chans[] = {PPT_TRANS_SYNC_CHANS};
    sync_channel_set(sizeof(chans) / sizeof(uint16_t), 1, chans);
#endif
}
