/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pos_ctrl.h
   * @brief     proprietary transport layer position control sub-module
   * @author
   * @date
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_POS_CTRL_H__
#define __PPT_TRANS_POS_CTRL_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "ppt_trans_handle.h"

/** @defgroup POSITION_CONTROL POSITION_CONTROL
  * @brief
  * @{
  */

/*============================================================================*
 *                              Definitions
 *============================================================================*/
/** @defgroup POSITION_CONTROL_Exported_Macros POSITION_CONTROL Exported Macros
 * @brief
 * @{
 */
#define PPT_TRANS_VECTOR_SUM_POS_VAL_MAX            (BIT(23) - 1) //int16_max
#define PPT_TRANS_VECTOR_SUM_POS_VAL_MIN            (BIT(23)) //int16_min
#define INT24_MASK                                  (BIT(24) - 1)

/*Define Section Start Example*/
/* This parameter determines the synchronized interval of position
displacement vector sums between 2.4G master and slave. For instance, if
this parameter is set to 5, it signifies that the 2.4G master will attempt to
transmit its vector sum every 5 packets. This synchronization interval allows
the master and slave devices to exchange information and ensure that their
displacement vectors are kept in harmony. By periodically transmitting the
vector sum, the master device keeps the slave device updated with the
latest displacement information. This allows for accurate tracking and
positioning of the cursor on the receiving end.
*/
#define PPT_TRANS_POS_CTRL_SYNC_INTERVAL             5
/*Define Section End Example*/
/** End of POSITION_CONTROL_Exported_Macros
  * @}
  */

/*============================================================================*
 *                         Variables
 *============================================================================*/
/** @defgroup POSITION_CONTROL_Exported_Variables POSITION_CONTROL Exported Variables
  * @brief
  * @{
  */
/*Variable Section Start Example*/
typedef union
{
    struct
    {
        int32_t pos_x;
        int32_t pos_y;
    };
    uint8_t d8[8];
} T_PPT_TRANS_POS_VECTOR_SUM;
/*Variable Section End Example*/
/** End of POSITION_CONTROL_Exported_Variables
  * @}
  */
/*============================================================================*
 *                              Functions
 *============================================================================*/
/** @defgroup POSITION_CONTROL_Exported_Functions POSITION_CONTROL Exported Functions
 * @brief
 * @{
 */
/*Code Section Start Example*/
/**
 * @brief  get the previous record position which store in the hist_pos array.
 * @param  idx - the recording index of the hist_pos array
 * @return the position record of the previous moment.
 */
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_get_hist_pos(uint8_t idx);

/**
 * @brief  update the record position directly.
 * @param  new_pos - the new position.
 */
void ppt_trans_pos_ctrl_update_pos(T_PPT_TRANS_POS_VECTOR_SUM new_pos);

/**
 * @brief  update record packet receiving anchor directly.
 * @param  seq - the sequence number of the received packet
 * @param  ce - the time anchor when receiving packet
 */
void ppt_trans_pos_ctrl_update_recv_ankor(uint8_t seq, uint16_t ce);

/**
 * @brief   update the record positon based on the received displacement vector.
 * @param   x - the position displacement on x-axis
 * @param   y - the position displacement on y-axis
 * @return  the updated position.
 */
T_PPT_TRANS_POS_VECTOR_SUM ppt_trans_pos_ctrl_recv_position_vector(int16_t x, int16_t y);

/**
 * @brief  judge whether the received packet is consecutively to prevent misalignment.
 * @param  seq - the sequence number of the received packet
 * @param  ce_cnt - the time anchor when receiving packet
 * @param  range - the tolerance for sequence skip
 * @return the validation result of the packet, 0 is invalid otherwise valid packet
 */
uint8_t ppt_trans_pos_ctrl_judge_pkt_valid(uint8_t seq, uint32_t ce_cnt, uint8_t range);

/**
 * @brief  position control module init, reset the parameter.
 * @return none
 * @retval void
 */
void ppt_trans_pos_ctrl_init(void);

/**
 * @brief  store - the currently position to the hist_pos array for future usage.
 * @param  idx - the recording index of the hist_pos array
 */
void ppt_trans_pos_ctrl_record_pos(uint8_t idx);

/**
 * @brief  reset record position history data which store in the hist_pos array.
 * @return none
 */
void ppt_trans_pos_ctrl_clear_hist(void);

/**
 * @brief   get the sequence number of previous valid received packet
 * @return  sequence number of packet
 */
uint8_t ppt_trans_pos_ctrl_get_prev_seq(void);

/**
 * @brief  reset the current position.
 * @return none
 */
void ppt_trans_pos_ctrl_clear_pos(void);

/**
 * @brief   handle positon control module when 2.4G master and slave disconnected.
 * @return  none
 */
void ppt_trans_pos_ctrl_handle_disconnect(void);

/**
 * @brief   handle positon control module when 2.4G master and slave connected.
 * @return  none
 */
void ppt_trans_pos_ctrl_handle_connect(void);

/**
 * @brief  ppt_trans_pos_ctrl_handle_recv_msg
 * @param  p_data - pointer to receive data.
 * @param  len - data length.
 * @param  rssi - ble rssi of data
 * @return none
 */
void ppt_trans_pos_ctrl_handle_recv_msg(uint8_t *p_data, uint16_t len,
                                        sync_receive_info_t *info);

/**
 * @brief register app callback for receiving data for position control module
 * @param  cb - callback function when packet received
 * @return none
*/
void ppt_trans_pos_ctrl_reg_receive_pos_cb(ppt_trans_handle_receive_pos_pkt_cb cb);

/**
 * @brief  handle positon control module when 2.4G master recv slave ack.
 * @param  seq - the sequence number of the packet
 * @param  is_pos_packet - indicate that whether the packet is a position packet or not
 * @return none
 */
void ppt_trans_pos_ctrl_handle_ack(uint8_t seq, bool is_pos_packet);


/**
 * @brief   handle positon control module when 2.4G master recv slave nack.
 * @return  none
 * @retval  void
 */
void ppt_trans_pos_ctrl_handle_nack(uint8_t seq, bool is_pos_packet);

/**
 * @brief  judge whether the packet should be change to position packet or not.
 * @param  seq - the sequence number of the packet
 * @param  u32_seq - the uint32_t sequence number of the packet
 * @return whether the packet should be change to position packet or not.
 * @retval true  packet need to be position packet.
 * @retval false packet no nned to be changed.
 */
bool ppt_trans_pos_ctrl_judge_force_pos_packet(uint8_t seq, uint32_t u32_seq);


/**
 * @brief   handle positon control module when 2.4G report rate is changed.
 * @return  none
 */
void ppt_trans_pos_ctrl_handle_report_rate_change(void);

/**
 * @brief  set position control force position parameter.
 * @param  msg_quota - the msg quota of sync lib
 * @retval none
 */
void ppt_trans_pos_ctrl_set_pos_pos_seq_gap(uint8_t msg_quota);
/*Code Section End Example*/
/** End of POSITION_CONTROL_Exported_Functions
  * @}
  */

/** End of POSITION_CONTROL
  * @}
  */

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_POS_CTRL_H__
