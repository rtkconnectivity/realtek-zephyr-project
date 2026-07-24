/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_sync_ctrl.h
   * @brief     2.4g stack/sync lib related callback functions
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_SYNC_CTRL_H__
#define __PPT_TRANS_SYNC_CTRL_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "os_timer.h"
#include "stdint.h"
#include "ppt_trans_handle.h"


/*============================================================================*
 *                              Variables
 *============================================================================*/
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
extern void *ppt_adp_guard_timer;
#endif

/*============================================================================*
 *                              Defines
 *============================================================================*/


typedef enum
{
    PPT_TRANS_SYNC_CTRL_NACK_ZERO     = 0,
    PPT_TRANS_SYNC_CTRL_NACK_ONE,
    PPT_TRANS_SYNC_CTRL_NACK_TWO,
    PPT_TRANS_SYNC_CTRL_NACK_THREE
} T_PPT_TRANS_SYNC_NACK_STATE;

typedef enum
{
    PPT_TRANS_SYNC_CTRL_ERR_CODE_SUCCESS  = 0,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_UNKNOWN,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_INVALID_LEN,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_INVALID_PARAM,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_INVALID_STATE,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_PKT_NULL_PTR,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_PKT_QUEUE_FULL,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_PKT_QUEUE_EMPTY,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_NO_MEM,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_NOT_FOUND,

    PPT_TRANS_SYNC_CTRL_ERR_CODE_SEND_RESULT_START = 0x20,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_PKT_NACKED,
    PPT_TRANS_SYNC_CTRL_ERR_CODE_PKT_FLUSHED,
} T_PPT_TRANS_SYNC_CTRL_ERR_CODE;

typedef struct
{
    int16_t  optical_x;
    int16_t  optical_y;
    uint8_t  fail_reason;
} T_PPT_TRANS_SYNC_CTRL_FAILED_PKT;

/*============================================================================*
 *                              Functions
 *============================================================================*/
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
/**
 * @brief restart report rate switch guarding timer
*/
static inline void ppt_trans_sync_ctrl_adp_guard_timer_enable(void)
{
    os_timer_restart(&ppt_adp_guard_timer, PPT_ADP_RPT_GUARD_TIME);
}

/**
 * @brief stop report rate switch guarding timer
 */
static inline void ppt_trans_sync_ctrl_adp_guard_timer_disable(void)
{
    os_timer_stop(&ppt_adp_guard_timer);
}
#endif // PPT_TRANS_FEATURE_ADP_RPT_RATE_EN

/******************************************************************
 * @brief  handle on-air data transmission
 * @retval void
 */
void ppt_trans_sync_ctrl_report_data(void);

/******************************************************************
 * @brief  initialize adaptive report rate feature parameters
 * @retval void
 */
void ppt_trans_sync_ctrl_init_adp_feature(void);

/******************************************************************
 * @brief register app callback for receiving data
 * @param  cb - callback function when packet received
*/
void ppt_trans_sync_ctrl_reg_receive_cb(ppt_trans_handle_receive_pkt_cb cb);

/******************************************************************
 * @brief register app callback for receiving raw data
 * @param  cb - callback function when packet received
*/
void ppt_trans_sync_ctrl_reg_receive_raw_cb(ppt_trans_handle_receive_raw_cb cb);

/******************************************************************
 * @brief reset sync layer callback to transport layer
*/
void ppt_trans_sync_ctrl_reset_receive_cb(void);

#if PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID
/**
 * @brief Configure pairing id used for sync lib
 *
 * @param pair_id
 * @return true - configure success
 * @return false - configure failed
 */
bool ppt_trans_sync_ctrl_set_pair_id(uint32_t pair_id);

/**
 * @brief Obtain pairing id used for sync lib
 *
 * @param pair_id
 * @return true - configure success
 * @return false - configure failed
 */
bool ppt_trans_sync_ctrl_get_pair_id(uint32_t *pair_id);
#endif

/******************************************************************
 * @brief init transport layer sync ctrl and register sync lib callbacks
*/
void ppt_trans_sync_ctrl_init(void);

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_SYNC_CTRL_H__
