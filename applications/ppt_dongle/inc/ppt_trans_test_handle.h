/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_test_handle.h
   * @brief     transport layer module test module handler
   * @author    luke
   * @date      2023-10-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_TEST_HANDLE__
#define __PPT_TRANS_TEST_HANDLE__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Test related macros
 *============================================================================*/
#define PPT_TRANS_TEST_ENABLE                0
#define PPT_TRANS_TEST_SYNC_REPORT_ITVL      100

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "ppt_trans_handle.h"


/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief init test module
*/
void ppt_trans_test_handle_init(void);

/**
 * @brief send pre-defined testcase out to replace real optical sensor data
*/
void ppt_trans_test_handle_get_testcase(T_PPT_TRANS_MOUSE_DATA *data);

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_TEST_HANDLE__ 
