/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file      board.h
* @brief     header file of ppt dongle demo.
* @details
* @author
* @date      2025-09-23
* @version   v0.1
* *********************************************************************************************************
*/


#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rtl_pinmux.h"

/*******************************************************
*                 Log Config
*******************************************************/
#define IS_RELEASE_VERSION      0   /* 1: close log    0: open log */

#define ENABLE_2_4G_LOG         0   /* 0: close log    1: open log */
#define BUTTON_PRINT_LOG        0   /* 0: close log    1: open log */

#define ENABLE_FULL_FEATURED_DIRECT_LOG     1

/*******************************************************
*                 2.4g Feature Config
*******************************************************/
#define PPT_PAIR_ACC                                            0x8EBE89D6

#define FEATURE_SUPPORT_APP_CFG_PPT_TX_POWER                    0  /* set 1 to use tx power that app config for ppt */
#define FEATURE_SUPPORT_PPT_QUEUE_SEND                          0  /* set 1 to enable ppt queue send strategy*/

/*******************************************************
*                 Dongle Repair Config
*******************************************************/
#define NOT_ALLOW_TO_REPAIR                             1
#define ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET 2

#define DONGLE_REPAIR_MODE  ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET

/*******************************************************
*                 USB Feature Config
*******************************************************/
#define FEATURE_SUPPORT_USB_FORCE_WAKE_UP_HOST                  1  /* set 1 to enable device force wake up host */
#define FEATURE_ALWAYS_IN_USB_MODE_WHEN_USB_INSET               1  /* set 1 to use usb mode when usb inset */

#endif /* _BOARD_H_ */

