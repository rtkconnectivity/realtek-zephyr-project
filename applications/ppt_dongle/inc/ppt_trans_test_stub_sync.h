/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_test_stub_sync.h
   * @brief     transport layer module test module for sync lib testing
   * @author    luke
   * @date      2023-10-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_TEST_STUB_SYNC__
#define __PPT_TRANS_TEST_STUB_SYNC__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "ppt_sync.h"
#include "ppt_trans_test_handle.h"

/*============================================================================*
 *                              Definitions
 *============================================================================*/
typedef enum
{
    PPT_TRANS_TEST_STUB_SYNC_PARAM_SUCCESS_MASK = 0x0,
    PPT_TRANS_TEST_STUB_SYNC_PARAM_REPORT_CNT,
    PPT_TRANS_TEST_STUB_SYNC_PARAM_REG_REPORT_CB,
} PPT_TRANS_TEST_STUB_SYNC_PARAMS;

typedef struct
{
    int32_t     motion_x;
    int32_t     motion_y;
    uint8_t     wheel_h_dir;
    uint8_t     wheel_v_dir;
    uint8_t     button_press[PPT_PKT_PAYLOAD_SIZE_BUTTON];
    uint8_t     button_release[PPT_PKT_PAYLOAD_SIZE_BUTTON];
} T_PPT_TRANS_TEST_STUB_SYNC_REPORT_FMT;


typedef void (*mouse_ppt_test_stub_sync_report_cb)(uint32_t test_cnt,
                                                   T_PPT_TRANS_TEST_STUB_SYNC_REPORT_FMT report_data);

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief stub-test submodule initializer
 *
 */
void ppt_trans_test_stub_sync_init(void);

/**
 * @brief stub-test submodule handler for send request from DUT module
 *
 * @param type - msg type, different types have different retransmit behavior
 * @param data - the data buffer
 * @param len - the data length
 * @param send_cb - the send complete callback
 */
void ppt_trans_test_stub_sync_send(sync_msg_type_t type, uint8_t *data, uint16_t len,
                                   sync_msg_send_cb_t send_cb);

/**
 * @brief stub-test submodule handler for testing parameter configuration from test handle module
 *
 * @param param_type - which parameter to configure @ref PPT_TRANS_TEST_STUB_SYNC_PARAMS
 * @param param_val  - specified parameter value
 */
void ppt_trans_test_stub_sync_set_test_param(PPT_TRANS_TEST_STUB_SYNC_PARAMS param_type,
                                             void *param_val);

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_TEST_STUB_SYNC__
