/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pkt_ctrl.h
   * @brief     proprietary transport layer packet data control sub-module
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_PKT_CTRL_H__
#define __PPT_TRANS_PKT_CTRL_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "ppt_trans_handle.h"

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  init transport layer packet control & managements
 * @retval void
 */
void ppt_trans_pkt_ctrl_init(void);

/******************************************************************
 * @brief  Receive mouse data send request from application layer
 * @param  mouse_data - mouse data to store
 * @param  is_dummy - mouse data is dummy or real data
 * @retval void
 */
void ppt_trans_pkt_ctrl_receive_data(T_PPT_TRANS_MOUSE_DATA mouse_data, bool is_dummy);

/******************************************************************
 * @brief  handle sending mouse position compensate data
 * @param  next_pkt - pointer that needs to copy packet data in
 * @param  length - how many bytes are copied in next_pkt
 * @param  wheel_dir - the direction of wheel sent in next_pkt
 * @retval pointer to the packet data to be transmitted
 */
void ppt_trans_pkt_ctrl_get_compensate_data(uint8_t *next_pkt, uint8_t *length, uint8_t *wheel_dir);

/******************************************************************
 * @brief  handle sending mouse offset data
 * @param  next_pkt - pointer that needs to copy packet data in
 * @param  length - how many bytes are copied in next_pkt
 * @param  wheel_dir - the direction of wheel sent in next_pkt
 * @retval pointer to the packet data to be transmitted
 */
void ppt_trans_pkt_ctrl_get_offset_data(uint8_t *next_pkt, uint8_t *length, uint8_t *wheel_dir);

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/******************************************************************
 * @brief  handle NACK packet wheel compensation
 * @param  wheel_dir - QDEC direction data
 *
 * @note   handle critical section outside this function
 */
void ppt_trans_pkt_ctrl_report_nack_wheel(uint8_t wheel_dir);

/******************************************************************
 * @brief  clear send fail/nack motion compensation
 */
void ppt_trans_pkt_ctrl_clear_compensate_motion(void);

/******************************************************************
 * @brief  handle NACK packet compensation
 * @param  motion_x - optical x-axis offset data
 * @param  motion_y - optical y-axis offset data
 */
void ppt_trans_pkt_ctrl_report_nack_motion(uint16_t motion_x, uint16_t motion_y);

/**
 * @brief   get the sequence number of packet
 * @return  sequence number of packet
 */
uint8_t ppt_trans_pkt_ctrl_get_cur_seq(void);

/**
 * @brief  Get current packet control module uint32_t packet sequence
 * @return  uint32_t sequence number of packet
 */
uint32_t ppt_trans_pkt_ctrl_get_u32_seq(void);

/******************************************************************
 * @brief  handle packet send fail compensation
 */
void ppt_trans_pkt_ctrl_report_send_fail_pkt(void);

/**
 * @brief  check whether wheel queue is empty.
 * @return the check result of wheel queue.
 * @retval bool
 */
bool ppt_trans_pkt_ctrl_get_wheel_is_send_cmpl(void);
#endif // PPT_TRANS_FEATURE_PKT_RETRANS_EN

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_PKT_CTRL_H__
