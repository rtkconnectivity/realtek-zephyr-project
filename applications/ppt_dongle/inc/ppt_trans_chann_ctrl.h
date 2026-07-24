/**
*****************************************************************************************
*     Copyright(c) 2024, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_chann_ctrl.h
   * @brief     proprietary transport layer channel control sub-module
   * @author
   * @date
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2024 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_CHANN_CTRL_H__
#define __PPT_TRANS_CHANN_CTRL_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "ppt_trans_handle.h"


/** @defgroup CHANNEL_CONTROL CHANNEL_CONTROL
  * @brief
  * @{
  */

/*============================================================================*
 *                              Definitions
 *============================================================================*/
/** @defgroup CHANNEL_CONTROL_Exported_Macros CHANNEL_CONTROL Exported Macros
 * @brief
 * @{
 */
/*Channel Control define Start Example*/
/*This parameter determines the interval at which the channel monitor is triggered.
It defines the duration, measured in milliseconds, between each activation of the channel monitor.*/
#define PPT_TRANS_CHANN_MONITOR_ITVL              1000

/*This parameter defines the minimum duration for channel usage in the channel control module.
It determines the amount of time the channel should be utilized after executing channel hopping.
The unit of measurement for this parameter is milliseconds.*/
#define PPT_TRANS_MIN_CHANN_USAGE_TIME            (2 * 1000)

/*This define the trigger threshold for the channel monitor. The parameter determines
the minimum acceptable ACK receive rate for the 2.4G packet transmission. This threshold
is defined as a percentage value. For example, if the trigger threshold is set to 98%, it
means that the 2.4G packet transmission system expects to receive at least 98% of the
ACKs from the 2.4G slave device. To put it in perspective, if 7000 packets are sent, the
system would expect to receive a minimum of 6860 ACKs.

By setting a higher trigger threshold, the system becomes more sensitive to the
environment and demands a higher success rate in terms of ACK reception. This can help
ensure a more reliable and efficient 2.4G packet transmission by minimizing the chances
of packet loss or interference. */
#define CHANN_MONITOR_CHANN_SWEEP_THRESH   95

/*This define the channel disqualify trigger counts threshold for the channel monitor. The
parameter determines the number of consecutive failures to meet the ACK receive rate
trigger threshold before a channel is viewed as disqualified. This threshold is specified by
a count value.

For example, if the disqualify trigger counts threshold is set to 3, it means that if a
channel fails to meet the ACK receive rate trigger threshold for 3 consecutive times, the
channel monitor will consider the channel disqualified and will execute channel sweeping to
select a new channel.

By setting a lower disqualify trigger counts threshold, the system will become more
sensitive to the environment and reacts quickly to unfavorable channel conditions.*/
#define SWEEP_TRIGGER_THRESH_COUNT         3

/*The parameter determines the duration for which the monitor scans and evaluates each
available channel during the sweeping operation. Users can adjust this duration based on
the size of the network and the time required for accurate channel analysis. The
parameter is defined in milliseconds.

For example, if the parameter is set to 200 milliseconds, it means that the channel
monitor will sample and evaluate each channel candidate for a period of 200 milliseconds.
During this time, the monitor collects statistical data and performance metrics for analysis
and decision-making.

By setting a larger value for the total accumulated time, the channel sweeping process will
take more time as each channel is evaluated for a longer duration. This can result in a
higher level of accuracy and reliability in determining the performance of each channel.*/
#define PER_CHANNEL_SWEEPING_TIME          200

/*The definition of this parameter is same as previous. The difference is that this
parameter is only used when the master and slave are performing reconnection*/
#define PER_RECONN_CHANNEL_SWEEPING_TIME   100

/*The definition of this parameter is same as previous. The difference is that this
parameter is only used when the master and slave first connected.*/
#define PER_CONN_CHANNEL_SWEEPING_TIME     500
/*Channel Control define End Example*/
/*Connect Sweep define Start Example*/
/*This define the lenth of the paratial set of the channel candidate. For example, by
setting the parameter to 4, the channel sweeping module will maintain a partial set
consisting of the best 4 channels. These channels will be considered as the channel
candidates when the 2.4GHz connection is lost and needs to be reestablished. The length
value shall smaller than the length of the full set channel candidates.
*/
#define LOST_RECONN_SWEEP_CHANN_LEN        4

/*This define the time threshold for channel monitor whether to use the partial set of the
channal candidates of not, the unit is define in milliseconds. For example, by setting the
parameters to 1500. When connected if the time delta since last disconnect is below 1500
ms, it may indicate that the environment has not changed significantly since the previous
connection, and the previous channel sweep result can be reused.*/
#define CONSIDER_LOST_DURATION             1500
/*Connect Sweep define End Example*/
/*Bad Environment define Start Example*/
/* These two parameters, BAD_ENVIRONMENT_MONITOR_TIME and
CONSIDER_BAD_ENVIRONMENT, work together to define the condition for rapid triggering.
BAD_ENVIRONMENT_MONITOR_TIME is defined in seconds and represents the time interval
within which the channel sweep triggers are monitored. It determines the duration over
which the channel sweep trigger count is evaluated.
CONSIDER_BAD_ENVIRONMENT is defined in count and represents the threshold for the
number of channel sweep triggers within the specified time interval. If the channel sweep
is triggered equal to or more than the specified count within the defined time interval, the
condition of rapid triggering is considered to be met.
For example, if the parameters are set to 20 seconds and 3 times, it means that the
channel sweep triggers are monitored within a 20-second interval. If the channel sweep is
triggered 3 times or more within this 20-second interval, it is considered to be a rapid
triggering condition and the module will lower the channel sweeping trigger threshold for
chaotic environment protection.
*/
#define BAD_ENVIRONMENT_MONITOR_TIME       20 //seconds
#define CONSIDER_BAD_ENVIRONMENT           3

/*The parameter determines the duration for which the channel monitor will lower the
channel sweeping trigger threshold when rapid triggering is detected. It is defined in
milliseconds.
For example, if you set the parameter to 60000 milliseconds (60 seconds), it means that
when the channel sweep is triggered rapidly and the condition for rapid triggering is met,
the channel monitor will lower the channel sweeping trigger threshold for a duration of 60
seconds.
*/
#define BAD_ENVIRONMENT_PROTECT_TIME       60000

/*The definition of this parameter is same as SWEEP_TRIGGER_THRESH_COUNT and is the
trigger criteria for chaotic environment.
*/
#define BAD_EVN_SWEEP_TRIGGER_THRESH       5

/*The definition of this parameter is same as CHANN_MONITOR_CHANN_SWEEP_THRESH
and is the trigger criteria for chaotic environment.
*/
#define BAD_EVN_CHANN_SWEEP_THRESH         90
/*Bad Environment define End Example*/
/*Channel Delta define Start Example*/
/* This parameter determines the minimn channel bandwidth delta with the emergency
channel and the main channel. For example if the main channel is 2442 (MHz) than the
emergency channel should greater than 2467 (Mhz) or lesser than 2417 (MHz)
*/
#define EMERGENCY_CAHNNEL_MIN_DELTA        25
/*Channel Delta define End Example*/
/** End of CHANNEL_CONTROL_Exported_Macros
  * @}
  */
/*============================================================================*
 *                              Functions
 *============================================================================*/
/** @defgroup CHANNEL_CONTROL_Exported_Functions CHANNEL_CONTROL Exported Functions
 * @brief
 * @{
 */
/*Code Section Start Example*/
/**
 * @brief  channel control module init, shall only called by 2.4G master.
 * @return none
 */
void ppt_trans_chann_ctrl_master_init(void);

/**
 * @brief  channel control module init, shall only called by 2.4G slave
 * @return none
 */
void ppt_trans_chann_ctrl_slave_init(void);

/**
 * @brief   command 2.4G master to perfoem channel sweep.
 * @param   len        - length of the channel index array
 * @param   chan_idx   - pointer of the channel index array
 * @param   sweep_time - period of time for each channel
 * @return  command success or fail
 * @retval  true    command success
 * @retval  false   command fail
 */
bool ppt_trans_chann_ctrl_command_sweep_channel(uint8_t len, uint8_t *chan_idx,
                                                uint16_t sweep_time);

/**
 * @brief   this api will process the statistic result of each channel when channel sweeping
 *          complete.
 * @param   param - the statistic result for each channel candidate
 * @return  none
 */
void ppt_trans_chann_ctrl_get_result(sync_chann_param_t param);

/**
 * @brief   handle channel control module when 2.4G master and slave disconnected.
 * @return  none
 */
void ppt_trans_chann_ctrl_handle_disconnect(void);

/**
 * @brief   handle channel control module when 2.4G master and slave connected.
 * @return  none
 */
void ppt_trans_chann_ctrl_handle_connect(void);

/**
 * @brief   To get whether 2.4G device is performing channel sweeping.
 * @return  channel sweeping status
 * @retval  true   no channel sweeping ongoing
 * @retval  false  channel sweeping ongoing
 */
bool ppt_trans_chann_ctrl_is_idle(void);

/**
 * @brief   ACK count statistic for channel monitor.
 * @return  none
 */
void ppt_trans_chann_ctrl_ack_cnt_inc(void);

/**
 * @brief   NACK count statistic for channel monitor.
 * @return  none
 */
void ppt_trans_chann_ctrl_nack_cnt_inc(void);

/**
 * @brief   Send fail count statistic for channel monitor.
 * @return  none
 */
void ppt_trans_chann_ctrl_send_fail_cnt_inc(void);

/**
* @brief   Declared for long packet module, will be referenced
*          when channel control long packet reception completes.
* @param   result   - long packet receive status.
* @param   pkt_data - pointer of received long packet data.
* @param   len      - length of received long packet data
* @param   info     - 2.4G related info.
* @return  none
*/
void ppt_tans_chann_ctrl_long_pkt_rcv_cb(bool result, uint8_t *pkt_data, uint16_t len,
                                         sync_receive_info_t info);

/**
 * @brief   handle channel control module when 2.4G slave disconnect with master,
 *          shall only be called by 2.4G slave.
 * @return  none
 */
void ppt_trans_chann_ctrl_slave_handle_disconnect(void);
/*Code Section End Example*/
/** End of CHANNEL_CONTROL_Exported_Functions
  * @}
  */

/** End of CHANNEL_CONTROL
  * @}
  */

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_8K_TRANS_LONG_PKT_CTRL_H__
