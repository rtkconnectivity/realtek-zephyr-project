/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_long_pkt_ctrl.h
   * @brief     proprietary transport layer long packet data sub-module
   * @author
   * @date
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_LONG_PKT_CTRL_H__
#define __PPT_TRANS_LONG_PKT_CTRL_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "ppt_trans_handle.h"


/*============================================================================*
 *                              Definitions
 *============================================================================*/

#define RCV_PKT_TABLE_MAX_LEN       128 // include length of CRC
#define SEND_PKT_TABLE_MAX_LEN      100


#define LONG_PKT_TYPE_FIRST_AND_END 0b00
#define LONG_PKT_TYPE_FIRST_PKT     0b01
#define LONG_PKT_TYPE_CONT_PKT      0b11
#define LONG_PKT_TYPE_END_PKT       0b10

#define RUNNING_WINDOW_MAX_LEN      32

typedef union _T_LONG_PKT_HEADER
{
    struct
    {
        uint8_t packet_type : 2;
        uint8_t seq : 6;
    };
    uint8_t d8;
} T_LONG_PKT_HEADER;

typedef struct _T_SEND_LONG_PKT_TABLE
{
    bool valid;
    uint8_t seq;
    uint16_t offset;
} T_SEND_LONG_PKT_TABLE;

typedef struct _T_RECV_LONG_PKT_TABLE
{
    bool valid;
    uint8_t seq;
    uint8_t len;
    uint8_t data[PPT_PKT_1K_M2S_PAYLOAD_SIZE_MAX - 1];
} T_RECV_LONG_PKT_TABLE;

typedef struct _T_RECV_REORG_LONG_PKT
{
    uint16_t len;
    uint8_t data[RCV_PKT_TABLE_MAX_LEN];
} T_RECV_REORG_LONG_PKT;

typedef enum
{
    USER_DEFINE_PKT = 0x0000,
    LINK_LAYER_PKT  = 0x0001,
    INVALID_HEADER  = 0xFFFF,
} T_LONG_PKT_TRANSFER_HEADER;

/*============================================================================*
 *                              Functions
 *============================================================================*/

/******************************************************************
 * @brief  init PPT transport layer long packet control & managements
 * @retval void
 */
void ppt_trans_long_pkt_ctrl_init(void);

/******************************************************************
 * @brief  register handler for long packet send completion or flushed
 * @param  pkt_src - pointer to the long packet data
 * @param  pkt_len - size of the long packet
 * @param  cb - callback function when long packet transmission completes
 * @retval void
 */
bool ppt_trans_long_pkt_ctrl_reg_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                               ppt_trans_handle_long_pkt_send_compl_cb cb);

/******************************************************************
 * @brief  register handler for long packet receive completion
 * @param  cb - callback function when long packet receiption completes
 * @retval void
 */
void ppt_trans_long_pkt_ctrl_reg_long_pkt_recv(ppt_trans_handle_long_pkt_recv_compl_cb cb);

/******************************************************************
 * @brief  handle long packet composition
 * @param  next_pkt - pointer of current processing packet
 * @param  len - size of current processing packet, unit bytes
 * @retval void
 */
bool ppt_trans_long_pkt_ctrl_get_data(uint8_t *next_pkt, uint8_t *len);

/******************************************************************
 * @brief  handle long packet parsing
 * @param  next_pkt - pointer of current processing packet
 * @param  len - size of , unit bytes
 * @param  info - indicate long packet recvived ce_count
 * @retval void
 */
void ppt_trans_long_pkt_parse_pkt(uint8_t *next_pkt, uint8_t len, sync_receive_info_t info);

/******************************************************************
 * @brief  handler function when packets that contains long pkt data is acked
 * @retval void
 */
void ppt_trans_long_pkt_recv_ack(uint8_t *header, sync_send_info_t info);

/******************************************************************
 * @brief  handle long packet send fail
 * @param  packet - pointer of the failed packet
 * @retval void
 */
void ppt_trans_long_pkt_handle_send_fail(uint8_t *packet);

/******************************************************************
 * @brief  To get whether the device is transmitting long packet or not.
 * @retval bool
 */
bool ppt_trans_long_pkt_get_is_long_pkt_transmmiting(void);

/******************************************************************
 * @brief   handle long packet module when 2.4G master and slave disconnected.
 * @return  none
 * @retval  void
 */
void ppt_trans_long_pkt_handle_connect(void);

/******************************************************************
 * @brief  To send link layer long packet message
 * @param  pkt_src - pointer of the packet
 * @param  pkt_len - size of , unit bytes
 * @param  cb - callback function when long packet send completes
 * @retval void
 */
bool ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                                      ppt_trans_handle_long_pkt_send_compl_cb cb);


#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_LONG_PKT_CTRL_H__
