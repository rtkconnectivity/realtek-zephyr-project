/**
*********************************************************************************************************
*               Copyright(c) 2015, Realtek Semiconductor Corporation. All rights reserved.
*********************************************************************************************************
* @file      board.h
* @brief     header file of Keypad demo.
* @details
* @author    tifnan_ge
* @date      2015-06-26
* @version   v0.1
* *********************************************************************************************************
*/


#ifndef _BOARD_H_
#define _BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif
/*============================================================================*
 *                              OTA Config
 *============================================================================*/
#define SUPPORT_NORMAL_OTA                          0
#define ENABLE_AUTO_BANK_SWITCH                     1 //for qc test
#define ENABLE_BANK_SWITCH_COPY_APP_DATA            1
#define ENABLE_SLAVE_REQUEST_UPDATE_COON_PARA       0

#define F_BT_LE_5_0_SET_PHY_SUPPORT                 0// check

#define SUPPORT_PUBLIC_DECODE_OTA                   0
#define SUPPORT_TEMP_COMBINED_OTA                   1

#define DFU_TEMP_BUFFER_SIZE                        2048
#define DFU_BUFFER_CHECK_ENABLE                     0x1//(g_ota_mode & 0x1)

/*when support soc check version, can select only support upgrade image version*/
#define OTA_ONLY_SUPPORT_UPGRADE                    0
/****************************************************/


#if (ENABLE_SLAVE_REQUEST_UPDATE_COON_PARA ==1)
/*interval 1.25ms/step,    supervision_timeout 10ms/step*/
#define CONNECT_INTERVAL_MIN                        0x6  /* (7.5ms) */
#define CONNECT_INTERVAL_MAX                        0x6  /* (7.5ms) */
#define CONNECT_LATENCY                             0x0
#define SUPERVISION_TIMEOUT                         1000 /* (10s) */
#endif
/****************************************************/


/*============================================================================*
*                        OTA configuration
*============================================================================*/
/*If support unsafe single bank ota user data, must define the following macros */
#define SUPPORT_SINGLE_BANK_OTA_USER_DATA
#ifdef SUPPORT_SINGLE_BANK_OTA_USER_DATA
#define USER_DATA_START_ADDR                       (0x00880000)
#define USER_DATA_MAX_SIZE                         (400 * 1024)  //400K
#define DISABLE_AES_OTA                             0
#endif

/*normal ota timeout settings*/
#define OTA_TIMEOUT_TOTAL                          240
#define OTA_TIMEOUT_WAIT4_CONN                     60
#define OTA_TIMEOUT_WAIT4_IMAGE_TRANS              200
#define OTA_TIMEOUT_CTITTV                         0xFF
#ifdef __cplusplus
}
#endif


/** @brief enable aon wdg which continue work in dlps state */
#define AON_WDG_ENABLE                             0
/** @brief set aon wdg timeout period in seconds, max value is 65s */
#define AON_WDG_TIME_OUT_PERIOD_SECOND             10

#endif  /* _BOARD_H_ */

