/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_long_pkt_ctrl.c
   * @brief     long packet data control module
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
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
#include "ppt_trans_chann_ctrl.h"
#endif
#include "ppt_trans_long_pkt_ctrl.h"
#include "ppt_trans_feat_ctrl.h"
#include "ppt_sync.h"

#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL

#define MIN(x,y)                      (x<y ? x:y)
#define MAX(x,y)                      (x>y ? x:y)
#define PKT_HEADER_LEN                2
#define LENTH_FIELD_LEN               1
#define EMER_FIELD_LEN                1
#define SWEEP_TIME_LEN                2
#define CRC_LEN                       2
#define U64_MAX                       0xFFFFFFFFFFFF

#define SWEEP_HEADER1_ADDR             0
#define SWEEP_HEADER2_ADDR             1
#define SWEEP_TIME_BYTE_LSB_ADDR       2
#define SWEEP_TIME_BYTE_MSB_ADDR       3
#define SWEEP_CHANNEL_LEN_ADDR         4
#define SWEEP_CHANNEL_CANDIDATE_ADDR   5
#define SWEEP_PKT_HEADER1              0xA5
#define SWEEP_PKT_HEADER2              0x5A

#define CHANGE_CHANNEL_HEADER1_ADDR    0
#define CHANGE_CHANNEL_HEADER2_ADDR    1
#define CHNAGE_CHANNEL_EMER_ADDR       2
#define CHNAGE_CHANNEL_LEN_ADDR        3
#define CHNAGE_CHANNEL_CANDIDATE_ADDR  4
#define CHANGE_PKT_HEADER1             0xA7
#define CHANGE_PKT_HEADER2             0x7A

#define DUMMY_INIT_HEADER1_ADDR        0
#define DUMMY_INIT_HEADER2_ADDR        1
#define DUMMY_INIT_PKT_HEADER1         0xA3
#define DUMMY_INIT_PKT_HEADER2         0x3A

#define CONN_DELAY_SWEEP               1000
#define CONN_PROTECT_OFFSET_MS         100
#define CONN_PROTECT_OFFSET_PACK       100

#define MAX_SWEEP_COUNT                7

extern bool ppt_reconnect(void);

typedef enum
{
    T_CHANN_CTRL_STATE_IDLE,
    T_CHANN_CTRL_STATE_BUSY,
} T_CHANN_CTRL_STATE;

typedef struct
{
    uint32_t stat;
    uint8_t idx;
} T_CHANN_SORT_STRUCT;

/*============================================================================*
 *                              Declares
 *============================================================================*/
static void ppt_trans_chann_ctrl_sweep_channel_cb(bool result,
                                                  sync_send_info_t info) RAM_FUNCTION;
static void ppt_trans_chann_ctrl_discon_reconn(TimerHandle_t p_timer) RAM_FUNCTION;
static void ppt_trans_chann_ctrl_change_channel_cb(bool result,
                                                   sync_send_info_t info) RAM_FUNCTION;
static void ppt_trans_chann_ctrl_command_change_chan(uint8_t len, uint8_t *idx,
                                                     uint8_t emergency_chan) RAM_FUNCTION;
bool ppt_trans_chann_ctrl_command_sweep_channel(uint8_t len, uint8_t *chan_idx,
                                                uint16_t sweep_time) RAM_FUNCTION;
void ppt_trans_chann_ctrl_get_result(sync_chann_param_t param) RAM_FUNCTION;
void ppt_tans_chann_ctrl_long_pkt_rcv_cb(bool result, uint8_t *pkt_data, uint16_t len,
                                         sync_receive_info_t info) RAM_FUNCTION;
/*============================================================================*
 *                              Static Variables
 *============================================================================*/
/** pointer to long packet data */
void *ppt_trans_chann_monitor_timer = NULL;
void *ppt_trans_slave_chann_protect_timer = NULL;
void *ppt_trans_bad_env_protect_timer = NULL;
static void ppt_trans_chann_monitor_report(TimerHandle_t p_timer) RAM_FUNCTION;
void *ppt_trans_delay_sweep_timer = NULL;
static uint8_t
*change_channel_pkt;// = {0xA7, 0x7A, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t  change_pkt_len = 0;
static uint8_t
*sweep_channel_pkt;// = {0xA5, 0x5A, 0xA7, 0x7A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t *dummy_init_pkt;// = {0xA3, 0x3A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static uint8_t dummy_init_pkt_len = 10;
static uint8_t sweep_pkt_len = 0;
static uint64_t prev_change_ankor = U64_MAX;
static T_CHANN_CTRL_STATE chann_state = T_CHANN_CTRL_STATE_IDLE;
static uint16_t ppt_trans_chann_ctrl_ack_cnt = 0;
static uint16_t ppt_trans_chann_ctrl_send_fail_cnt = 0;
static uint16_t ppt_trans_chann_ctrl_nack_cnt = 0;
static uint32_t ppt_min_chann_usage_time = 0;
static uint8_t ppt_fail_trigger_thresh_count = 0;
static uint8_t converge_channel_num = 0xff;
static uint8_t total_chann_len = 0xff;
static uint8_t lost_reconn_sweep_chann_len = 0xff;
static bool lost_reconn_sweep_chann_rdy = false;
static uint8_t *lost_reconn_sweep_chann;
static uint64_t disconnect_ankor = U64_MAX;
static uint8_t trigger_ankor[BAD_ENVIRONMENT_MONITOR_TIME];
static uint8_t trigger_count = 0;
static uint8_t trigger_read_idx = 0;
static uint8_t trigger_write_idx = 0;
static uint8_t chan_monitor_sweep_thresh = 0;
static uint8_t monitor_stat_fail_cnt = 0;
static uint8_t sweep_counter_remain = MAX_SWEEP_COUNT;


/******************************************************************
 * @brief  Timer callback for monitor packet transmission quality
 * @param  p_timer - not used
 * @return none
 * @retval void
 */
static void ppt_trans_chann_monitor_report(TimerHandle_t p_timer)
{

    uint32_t s = os_lock();
    uint16_t temp_send_fail_cnt = ppt_trans_chann_ctrl_send_fail_cnt;
    uint16_t temp_ack_cnt = ppt_trans_chann_ctrl_ack_cnt;
    uint16_t temp_nack_cnt = ppt_trans_chann_ctrl_nack_cnt;
    os_unlock(s);

    uint16_t total_cnt = temp_send_fail_cnt + temp_ack_cnt + temp_nack_cnt;
    uint32_t ppt_interval = 125;
    sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);

    uint16_t sweep_thresh;
    if (ppt_interval == 125)
    {
        sweep_thresh = ((total_cnt - (total_cnt >> 3)) * chan_monitor_sweep_thresh) / 100;
    }
    else
    {
        sweep_thresh = (total_cnt * chan_monitor_sweep_thresh) / 100;
    }

    trigger_ankor[trigger_write_idx] = 0;
    if (temp_ack_cnt < sweep_thresh)
    {
        monitor_stat_fail_cnt++;
        if (ppt_fail_trigger_thresh_count <= monitor_stat_fail_cnt)
        {
            uint64_t cur = os_sys_time_get();
            uint64_t t_delta = cur > prev_change_ankor ? cur - prev_change_ankor :
                               (U64_MAX - prev_change_ankor) + cur;
            if (t_delta >= ppt_min_chann_usage_time)
            {
                prev_change_ankor = cur;
                uint16_t chann[] = {PPT_TRANS_SYNC_CHANS};
                uint8_t len = sizeof(chann) / sizeof(uint16_t);
                uint8_t *chann_idx = os_mem_alloc(RAM_TYPE_DATA_ON, len);
                uint8_t *ptr = chann_idx;
                for (uint8_t i = 0; i < len; i++)
                {
                    *ptr = i;
                    ptr++;
                }
                uint16_t sweep_time = change_channel_pkt[SWEEP_CHANNEL_LEN_ADDR] * PER_CHANNEL_SWEEPING_TIME;

                if (len != 0 && chann_state == T_CHANN_CTRL_STATE_IDLE)
                {
                    ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(dummy_init_pkt, dummy_init_pkt_len,
                                                                     NULL);
                    ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(dummy_init_pkt, dummy_init_pkt_len,
                                                                     NULL);
                }
                bool ret = ppt_trans_chann_ctrl_command_sweep_channel(len, chann_idx, sweep_time);
                os_mem_free(chann_idx);

                if (ret)
                {
                    monitor_stat_fail_cnt = 0;
                    trigger_ankor[trigger_write_idx] = 1;
                    trigger_count += 1;
                    sweep_counter_remain = MAX_SWEEP_COUNT;
                }

                if (trigger_count >= CONSIDER_BAD_ENVIRONMENT)
                {
                    os_timer_start(&ppt_trans_bad_env_protect_timer);
                    chan_monitor_sweep_thresh = BAD_EVN_CHANN_SWEEP_THRESH;
                    ppt_fail_trigger_thresh_count = BAD_EVN_SWEEP_TRIGGER_THRESH;
                }
            }
        }
    }
    else
    {
        monitor_stat_fail_cnt = 0;
    }

    trigger_write_idx = (trigger_write_idx + 1) % BAD_ENVIRONMENT_MONITOR_TIME;

    if (trigger_read_idx == trigger_write_idx)
    {
        trigger_count -= trigger_ankor[trigger_read_idx];
        trigger_read_idx = (trigger_read_idx + 1) % BAD_ENVIRONMENT_MONITOR_TIME;
    }

    APP_PRINT_TRACE4("[ppt_trans_chann_monitor_report] sweep trigger %d, ack count %d, total send count %d, %d",
                     sweep_thresh, temp_ack_cnt, total_cnt, trigger_count);
    ppt_trans_chann_ctrl_send_fail_cnt = 0;
    ppt_trans_chann_ctrl_nack_cnt = 0;
    ppt_trans_chann_ctrl_ack_cnt = 0;
}

/******************************************************************
 * @brief  Timer callback for monitor environment quality,
 *         will lower channel sweep thresh if environment is chaotic
 * @param  p_timer - not used
 * @return none
 * @retval void
 */
static void ppt_trans_bad_env_protect_report(TimerHandle_t p_timer)
{
    chan_monitor_sweep_thresh = CHANN_MONITOR_CHANN_SWEEP_THRESH;
    ppt_fail_trigger_thresh_count = SWEEP_TRIGGER_THRESH_COUNT;
}

/******************************************************************
 * @brief  Timer callback for 2.4G slave to prevent channel sweep command misalign
 * @param  p_timer - not used
 * @return none
 * @retval void
 */
static void ppt_trans_chann_ctrl_discon_reconn(TimerHandle_t p_timer)
{
    APP_PRINT_TRACE0("[ppt_trans_chann_ctrl_discon_reconn] Protection");
    sync_stop();
    ppt_reconnect();
}

/******************************************************************
 * @brief  Callback function when channel sweep command send complete
 * @param  result - command send status
 * @param  info - 2.4G related info
 * @return none
 * @retval void
 */
static void ppt_trans_chann_ctrl_sweep_channel_cb(bool result, sync_send_info_t info)
{
    APP_PRINT_TRACE2("[ppt_trans_chann_ctrl_sweep_channel_cb] sweep pkt send cmpl result, %d, data = %b",
                     result, TRACE_BINARY(sweep_channel_pkt[SWEEP_CHANNEL_LEN_ADDR],
                                          &sweep_channel_pkt[SWEEP_CHANNEL_CANDIDATE_ADDR]));
    if (!result)
    {
        chann_state = T_CHANN_CTRL_STATE_IDLE;
        ppt_trans_chann_ctrl_discon_reconn(NULL);
        return;
    }
    uint16_t sweep_time = ((sweep_channel_pkt[SWEEP_TIME_BYTE_MSB_ADDR] << 8) |
                           sweep_channel_pkt[SWEEP_TIME_BYTE_LSB_ADDR]);
    uint32_t sweep_ce_cnt = sweep_time;
    uint32_t ppt_interval = 125;
    sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
    if (ppt_interval != 125)
    {
        sweep_ce_cnt *= 1000;           // transform to us
        sweep_ce_cnt /= ppt_interval;   // transform to ce cnt
    }

    sync_channel_perform_channel_sweep(info.ce_count, &sweep_channel_pkt[SWEEP_CHANNEL_CANDIDATE_ADDR],
                                       sweep_channel_pkt[SWEEP_CHANNEL_LEN_ADDR], (uint16_t)sweep_ce_cnt);
    return;
}

/******************************************************************
 * @brief  Callback function when channel change command send complete
 * @param  result - command send status
 * @param  info - 2.4G related info
 * @return none
 * @retval void
 */
static void ppt_trans_chann_ctrl_change_channel_cb(bool result, sync_send_info_t info)
{
    APP_PRINT_TRACE2("[ppt_trans_chann_ctrl_change_channel_cb] change pkt send cmpl result %d, data = %b",
                     result, TRACE_BINARY(change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR],
                                          &change_channel_pkt[CHNAGE_CHANNEL_CANDIDATE_ADDR]));
    if (!result)
    {
        chann_state = T_CHANN_CTRL_STATE_IDLE;
        ppt_trans_chann_ctrl_discon_reconn(NULL);
        return;
    }
    ppt_fail_trigger_thresh_count = SWEEP_TRIGGER_THRESH_COUNT;
    sync_channel_perform_change(info.ce_count, &change_channel_pkt[CHNAGE_CHANNEL_CANDIDATE_ADDR],
                                change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR]);
    chann_state = T_CHANN_CTRL_STATE_IDLE;
    prev_change_ankor = os_sys_time_get();

    if (change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR] > 1)
    {
        uint16_t sweep_time = change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR] * PER_CHANNEL_SWEEPING_TIME;
        ppt_trans_chann_ctrl_command_sweep_channel(change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR],
                                                   &change_channel_pkt[CHNAGE_CHANNEL_CANDIDATE_ADDR], sweep_time);
        return;
    }
    else
    {
        uint16_t chann[] = {PPT_TRANS_SYNC_CHANS};
        APP_PRINT_TRACE2("[ppt_trans_chann_ctrl_change_channel_cb] main ch: %d, emergency chann %d",
                         chann[change_channel_pkt[CHNAGE_CHANNEL_CANDIDATE_ADDR]],
                         chann[change_channel_pkt[CHNAGE_CHANNEL_EMER_ADDR]]);
        sync_channel_ctrl_set_emergency_channel(change_channel_pkt[CHNAGE_CHANNEL_EMER_ADDR]);
        sync_channel_statistics_clear();
    }

    return;
}

/******************************************************************
 * @brief  Send out channel change link layer command
 * @param  len - lenght of the new channel index array
 * @param  idx - the start pointer of new channel index array
 * @param  emergency_chan - emergency_chan index
 * @return none
 * @retval void
 */
static void ppt_trans_chann_ctrl_command_change_chan(uint8_t len, uint8_t *idx,
                                                     uint8_t emergency_chan)
{
    change_channel_pkt[CHNAGE_CHANNEL_EMER_ADDR] = emergency_chan;
    change_channel_pkt[CHNAGE_CHANNEL_LEN_ADDR] = len;
    memcpy(&change_channel_pkt[CHNAGE_CHANNEL_CANDIDATE_ADDR], idx, len);
    uint8_t to_send_len = MAX(len + CRC_LEN + LENTH_FIELD_LEN + EMER_FIELD_LEN, 5);
    bool ret = ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(change_channel_pkt, to_send_len,
                                                                ppt_trans_chann_ctrl_change_channel_cb);
    if (!ret)
    {
        chann_state = T_CHANN_CTRL_STATE_IDLE;
    }
}

/******************************************************************
 * @brief  Timer callback for perform channel sweeping when 2.4G connected
 * @param  result - command send status
 * @param  info - 2.4G related info
 * @return none
 * @retval void
 */
static void ppt_trans_chann_ctrl_handle_connect_delay_sweep(TimerHandle_t p_timer)
{
    chann_state = T_CHANN_CTRL_STATE_IDLE;
    prev_change_ankor = os_sys_time_get();
    uint8_t chan_len = total_chann_len;

    uint8_t *chann_idx = os_mem_alloc(RAM_TYPE_DATA_ON, total_chann_len);
    uint8_t *ptr = chann_idx;
    for (uint8_t i = 0; i < total_chann_len; i++)
    {
        *ptr = i;
        ptr++;
    }


    uint64_t cur = os_sys_time_get();
    uint16_t sweep_time = chan_len * PER_CONN_CHANNEL_SWEEPING_TIME;
    if (lost_reconn_sweep_chann_rdy && cur > disconnect_ankor &&
        cur - disconnect_ankor < (CONSIDER_LOST_DURATION + CONN_DELAY_SWEEP))
    {
        memcpy(chann_idx, lost_reconn_sweep_chann, lost_reconn_sweep_chann_len);
        chan_len = lost_reconn_sweep_chann_len;
        sweep_time = chan_len * PER_RECONN_CHANNEL_SWEEPING_TIME;
    }
    if (chan_len != 0 && chann_state == T_CHANN_CTRL_STATE_IDLE)
    {
        ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(dummy_init_pkt, dummy_init_pkt_len,
                                                         NULL);
        ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(dummy_init_pkt, dummy_init_pkt_len,
                                                         NULL);
    }
    bool ret = ppt_trans_chann_ctrl_command_sweep_channel(chan_len, chann_idx, sweep_time);
    if (ret)
    {
        sweep_counter_remain = MAX_SWEEP_COUNT;
    }
    os_mem_free(chann_idx);
    os_timer_restart(&ppt_trans_chann_monitor_timer, PPT_TRANS_SYNC_CTRL_REPORT_STAT_ITVL);
}
/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  channel control module init, shall only called by 2.4G master
 * @return none
 * @retval void
 */
void ppt_trans_chann_ctrl_master_init(void)
{
    if (false == os_timer_create(&ppt_trans_chann_monitor_timer,
                                 "chann monitor timer", 0, PPT_TRANS_CHANN_MONITOR_ITVL,
                                 true, ppt_trans_chann_monitor_report))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] stat timer creation failed");
    }

    if (false == os_timer_create(&ppt_trans_bad_env_protect_timer,
                                 "ppt_trans_bad_env_protect_timer", 0, BAD_ENVIRONMENT_PROTECT_TIME,
                                 false, ppt_trans_bad_env_protect_report))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] env timer creation failed");
    }

    if (false == os_timer_create(&ppt_trans_delay_sweep_timer,
                                 "ppt_trans_delay_sweep_timer", 0, CONN_DELAY_SWEEP,
                                 false, ppt_trans_chann_ctrl_handle_connect_delay_sweep))
    {
        APP_PRINT_ERROR0("[ppt_trans_sync_ctrl_init] delay_sweep_timer creation failed");
    }

    ppt_fail_trigger_thresh_count = SWEEP_TRIGGER_THRESH_COUNT;
    uint16_t chann[] = {PPT_TRANS_SYNC_CHANS};
    total_chann_len = sizeof(chann) / sizeof(uint16_t);

    lost_reconn_sweep_chann_len = MIN(LOST_RECONN_SWEEP_CHANN_LEN, total_chann_len);
    lost_reconn_sweep_chann = os_mem_alloc(RAM_TYPE_DATA_ON, lost_reconn_sweep_chann_len);

    change_pkt_len = total_chann_len + PKT_HEADER_LEN + CRC_LEN + LENTH_FIELD_LEN + EMER_FIELD_LEN;
    sweep_pkt_len = total_chann_len + PKT_HEADER_LEN + SWEEP_TIME_LEN + CRC_LEN + LENTH_FIELD_LEN;

    change_channel_pkt = os_mem_alloc(RAM_TYPE_DATA_ON, change_pkt_len);
    sweep_channel_pkt = os_mem_alloc(RAM_TYPE_DATA_ON, sweep_pkt_len);
    dummy_init_pkt = os_mem_alloc(RAM_TYPE_DATA_ON, dummy_init_pkt_len);

    change_channel_pkt[CHANGE_CHANNEL_HEADER1_ADDR] = CHANGE_PKT_HEADER1;
    change_channel_pkt[CHANGE_CHANNEL_HEADER2_ADDR] = CHANGE_PKT_HEADER2;

    sweep_channel_pkt[SWEEP_HEADER1_ADDR] = SWEEP_PKT_HEADER1;
    sweep_channel_pkt[SWEEP_HEADER2_ADDR] = SWEEP_PKT_HEADER2;

    dummy_init_pkt[DUMMY_INIT_HEADER1_ADDR] = DUMMY_INIT_PKT_HEADER1;
    dummy_init_pkt[DUMMY_INIT_HEADER2_ADDR] = DUMMY_INIT_PKT_HEADER2;

    ppt_min_chann_usage_time = PPT_TRANS_MIN_CHANN_USAGE_TIME;

    memset(trigger_ankor, 0, BAD_ENVIRONMENT_MONITOR_TIME);

    chan_monitor_sweep_thresh = CHANN_MONITOR_CHANN_SWEEP_THRESH;

    bool ret;
    ret = sync_channel_ctrl_stat_cmpl_register(ppt_trans_chann_ctrl_get_result,
                                               PER_CHANNEL_SWEEPING_TIME);
    ppt_trans_feat_ctrl_set_feature_compatible(T_PPT_TRANS_FEAT_CTRL_FEATURE_CHAN_CTRL, ret);
}

/******************************************************************
 * @brief  channel control module init, shall only called by 2.4G slave
 * @return none
 * @retval void
 */
void ppt_trans_chann_ctrl_slave_init(void)
{
    if (false == os_timer_create(&ppt_trans_slave_chann_protect_timer,
                                 "slave chann protect", 0, PPT_TRANS_CHANN_MONITOR_ITVL,
                                 false, ppt_trans_chann_ctrl_discon_reconn))
    {
        APP_PRINT_ERROR0("[ppt_trans_chann_ctrl_slave_init] slave chann protect creation failed");
    }
}

/******************************************************************
 * @brief   command 2.4G master to perfoem channel sweep.
 * @param   len        - length of the channel index array
 * @param   chan_idx   - pointer of the channel index array
 * @param   sweep_time - period of time for each channel
 * @return  command success or fail
 * @retval  bool
 */
bool ppt_trans_chann_ctrl_command_sweep_channel(uint8_t len, uint8_t *chan_idx,
                                                uint16_t sweep_time)
{
    if (len != 0 && chann_state == T_CHANN_CTRL_STATE_IDLE)
    {
        converge_channel_num = 0xff;
        sweep_channel_pkt[SWEEP_TIME_BYTE_LSB_ADDR] = (uint8_t)sweep_time;
        sweep_channel_pkt[SWEEP_TIME_BYTE_MSB_ADDR] = sweep_time >> 8;
        sweep_channel_pkt[SWEEP_CHANNEL_LEN_ADDR] = len;
        uint8_t *ptr = &sweep_channel_pkt[SWEEP_CHANNEL_CANDIDATE_ADDR];
        for (uint16_t i = 0; i < len; i++)
        {
            *ptr = chan_idx[i];
            ptr++;
        }

        uint8_t to_send_len = len + SWEEP_CHANNEL_CANDIDATE_ADDR;


        bool ret = ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(sweep_channel_pkt, to_send_len,
                                                                    ppt_trans_chann_ctrl_sweep_channel_cb);
        if (ret)
        {
            chann_state = T_CHANN_CTRL_STATE_BUSY;
        }
        return ret;
    }
    return false;
}

/******************************************************************
 * @brief   this api will process the statistic result of each channel when channel sweeping
 *          complete.
 * @param   param - the statistic result for each channel candidate
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_get_result(sync_chann_param_t param)
{
    uint16_t chan[] = {PPT_TRANS_SYNC_CHANS};
    uint32_t failed_num = 0xFFFFFFFF;
    for (uint8_t i = 0; i < param.channel_candidate_len; i++)
    {
        if (param.channel_fail_statistic[param.channel_candidate[i]] < failed_num)
        {
            failed_num = param.channel_fail_statistic[param.channel_candidate[i]];
        }
    }
    uint8_t *temp_buff = os_mem_alloc(RAM_TYPE_DATA_ON, param.channel_candidate_len);
    uint8_t *ptr = temp_buff;
    uint8_t count = 0;
    uint8_t *sort_buffer = os_mem_alloc(RAM_TYPE_DATA_ON,
                                        sizeof(T_CHANN_SORT_STRUCT) * param.channel_candidate_len);
    T_CHANN_SORT_STRUCT *cast_ptr = (T_CHANN_SORT_STRUCT *) sort_buffer;

    for (uint8_t i = 0; i < param.channel_candidate_len; i++)
    {
        if (param.channel_fail_statistic[param.channel_candidate[i]] <= failed_num + 5)
        {
            *ptr = param.channel_candidate[i];
            ptr++;
            count++;
#if PPT_TRANS_CHANN_CTRL_DBG_LOG
            APP_PRINT_TRACE5("[chann_ctrl_stat_result] detail dump %d %d %d, %d %d", count, failed_num,
                             param.channel_fail_statistic[param.channel_candidate[i]], chan[param.channel_candidate[i]],
                             param.channel_candidate[i]);
#endif
        }
        else
        {
#if PPT_TRANS_CHANN_CTRL_DBG_LOG
            APP_PRINT_TRACE5("[chann_ctrl_stat_result] fail detail dump %d %d %d, %d %d", count, failed_num,
                             param.channel_fail_statistic[param.channel_candidate[i]], chan[param.channel_candidate[i]],
                             param.channel_candidate[i]);
#endif
        }
        cast_ptr[i].idx = param.channel_candidate[i];
        cast_ptr[i].stat = param.channel_fail_statistic[param.channel_candidate[i]];
    }


    uint8_t emergency_chann = 0xff;

    for (uint8_t i = 0; i < param.channel_candidate_len; i++)
    {
        for (uint8_t j = i + 1; j < param.channel_candidate_len; j++)
        {
            if (cast_ptr[i].stat > cast_ptr[j].stat)
            {
                T_CHANN_SORT_STRUCT temp = cast_ptr[j];
                cast_ptr[j] = cast_ptr[i];
                cast_ptr[i] = temp;
            }
        }
    }

    uint16_t chann[] = {PPT_TRANS_SYNC_CHANS};
    for (uint8_t i = 1; i < param.channel_candidate_len; i++)
    {
        if (abs(chann[cast_ptr[i].idx] - chann[cast_ptr[0].idx]) >= EMERGENCY_CAHNNEL_MIN_DELTA)
        {
            emergency_chann = cast_ptr[i].idx;
            break;
        }
    }

    if (param.channel_candidate_len >= lost_reconn_sweep_chann_len)
    {
        for (uint8_t i = 0; i < lost_reconn_sweep_chann_len; i++)
        {
            lost_reconn_sweep_chann[i] = cast_ptr[i].idx;
        }

        if (emergency_chann == 0xff)
        {
            emergency_chann = cast_ptr[1].idx;
        }
        lost_reconn_sweep_chann_rdy = true;
#if PPT_TRANS_CHANN_CTRL_DBG_LOG
        APP_PRINT_TRACE2("[chann_ctrl_stat_result] can shrink %d, %b", lost_reconn_sweep_chann_len,
                         TRACE_BINARY(lost_reconn_sweep_chann_len, lost_reconn_sweep_chann));
#endif
    }
    else if (param.channel_candidate_len >= 2)
    {
        if (emergency_chann == 0xff)
        {
            emergency_chann = cast_ptr[1].idx;
        }
    }
#if PPT_TRANS_CHANN_CTRL_DBG_LOG
    for (uint8_t i = 0; i < param.channel_candidate_len; i++)
    {
        APP_PRINT_TRACE2("[chann_ctrl_stat_result] sort %d %d", cast_ptr[i].idx, cast_ptr[i].stat);
    }
#endif
    if (sweep_counter_remain == 0)
    {
        count = 1;
        temp_buff[0] = cast_ptr[0].idx;
    }
    else
    {
        sweep_counter_remain--;
    }
    os_mem_free(sort_buffer);

    if (converge_channel_num == count)
    {
        os_mem_free(temp_buff);
        return;
    }
#if PPT_TRANS_CHANN_CTRL_DBG_LOG
    APP_PRINT_TRACE4("[chann_ctrl_stat_result] pkt dump %d %d %d, %b", converge_channel_num, count,
                     param.channel_candidate_len, TRACE_BINARY(param.channel_candidate_len, temp_buff));
#endif
    converge_channel_num = count;
    if (count != 1)
    {
        emergency_chann = 0xff;
    }
    ppt_trans_chann_ctrl_command_change_chan(count, temp_buff, emergency_chann);
    os_mem_free(temp_buff);
}

/******************************************************************
 * @brief   To get whether 2.4G device is performing channel sweeping.
 * @return  none
 * @retval  bool
 */
bool ppt_trans_chann_ctrl_is_idle(void)
{
    return (chann_state == T_CHANN_CTRL_STATE_IDLE);
}

/******************************************************************
 * @brief   handle channel control module when 2.4G master and slave disconnected.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_handle_disconnect(void)
{
    monitor_stat_fail_cnt = 0;
    chann_state = T_CHANN_CTRL_STATE_IDLE;
    disconnect_ankor = os_sys_time_get();
    os_timer_stop(&ppt_trans_chann_monitor_timer);
}

/******************************************************************
 * @brief   handle channel control module when 2.4G slave disconnect with master,
            shall only called by 2.4G slave.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_slave_handle_disconnect(void)
{
    os_timer_stop(&ppt_trans_slave_chann_protect_timer);
}

/******************************************************************
 * @brief   handle channel control module when 2.4G master and slave connected.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_handle_connect(void)
{
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_CHAN_CTRL) == true)
    {
        os_timer_start(&ppt_trans_delay_sweep_timer);
    }
    else
    {
        APP_PRINT_WARN0("[ppt_trans_chann_ctrl_handle_connect] chann ctrl feature is disabled");
    }
}

/******************************************************************
 * @brief   ACK count statistic for channel monitor.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_ack_cnt_inc(void)
{
    ppt_trans_chann_ctrl_ack_cnt++;
}

/******************************************************************
 * @brief   NACK count statistic for channel monitor.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_nack_cnt_inc(void)
{
    ppt_trans_chann_ctrl_nack_cnt++;
}

/******************************************************************
 * @brief   Send fail count statistic for channel monitor.
 * @return  none
 * @retval  void
 */
void ppt_trans_chann_ctrl_send_fail_cnt_inc(void)
{
    ppt_trans_chann_ctrl_send_fail_cnt++;
}

/******************************************************************
* @brief   For long packet module, will call when channel control long packet receive
*          complete.
* @param   result   - long packet receive status.
* @param   pkt_data - pointer of received long packet data.
* @param   len      - length of received long packet data
* @param   info     - 2.4G related info.
* @return  none
* @retval  void
*/
void ppt_tans_chann_ctrl_long_pkt_rcv_cb(bool result, uint8_t *pkt_data, uint16_t len,
                                         sync_receive_info_t info)
{
    if (result && len >= 3)
    {
        if (pkt_data[SWEEP_HEADER1_ADDR] == SWEEP_PKT_HEADER1 &&
            pkt_data[SWEEP_HEADER2_ADDR] == SWEEP_PKT_HEADER2)
        {
            uint16_t sweep_time = ((pkt_data[SWEEP_TIME_BYTE_MSB_ADDR] << 8) |
                                   pkt_data[SWEEP_TIME_BYTE_LSB_ADDR]);
            uint32_t sweep_ce_cnt = sweep_time;
            uint32_t offset_ms = CONN_PROTECT_OFFSET_MS;
            uint32_t ppt_interval = 125;
            sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
            if (ppt_interval != 125)
            {
                sweep_ce_cnt *= 1000;           // transform to us
                sweep_ce_cnt /= ppt_interval;   // transform to ce cnt
                if (ppt_interval >= 1000)
                {
                    offset_ms = CONN_PROTECT_OFFSET_PACK * (1000 / (1000000 / ppt_interval));
                }
            }
            sync_channel_perform_channel_sweep(info.ce_count,
                                               &pkt_data[SWEEP_CHANNEL_CANDIDATE_ADDR],
                                               pkt_data[SWEEP_CHANNEL_LEN_ADDR], (uint16_t)sweep_ce_cnt);
            os_timer_restart(&ppt_trans_slave_chann_protect_timer,
                             sweep_time + offset_ms);
        }
        else if (pkt_data[CHANGE_CHANNEL_HEADER1_ADDR] == CHANGE_PKT_HEADER1 &&
                 pkt_data[CHANGE_CHANNEL_HEADER2_ADDR] == CHANGE_PKT_HEADER2)
        {
            sync_channel_perform_change(info.ce_count,
                                        &pkt_data[CHNAGE_CHANNEL_CANDIDATE_ADDR],
                                        pkt_data[CHNAGE_CHANNEL_LEN_ADDR]);
            sync_channel_ctrl_set_emergency_channel(pkt_data[CHNAGE_CHANNEL_EMER_ADDR]);
            uint32_t offset_ms = CONN_PROTECT_OFFSET_MS;
            uint32_t ppt_interval = 125;
            sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
            if (ppt_interval >= 1000)
            {
                offset_ms = CONN_PROTECT_OFFSET_PACK * (1000 / (1000000 / ppt_interval));
            }

            if (pkt_data[CHNAGE_CHANNEL_LEN_ADDR] == 1)
            {
                os_timer_stop(&ppt_trans_slave_chann_protect_timer);
            }
            else
            {
                os_timer_restart(&ppt_trans_slave_chann_protect_timer, offset_ms);
            }
        }
        else if (pkt_data[DUMMY_INIT_HEADER1_ADDR] == DUMMY_INIT_PKT_HEADER1 &&
                 pkt_data[DUMMY_INIT_HEADER2_ADDR] == DUMMY_INIT_PKT_HEADER2)
        {
            uint32_t offset_ms = CONN_PROTECT_OFFSET_MS;
            uint32_t ppt_interval = 125;
            sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
            if (ppt_interval >= 1000)
            {
                offset_ms = CONN_PROTECT_OFFSET_PACK * (1000 / (1000000 / ppt_interval));
            }
            os_timer_stop(&ppt_trans_slave_chann_protect_timer);
            os_timer_restart(&ppt_trans_slave_chann_protect_timer, offset_ms);
        }
    }
}
#endif
