/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pkt_algo.h
   * @brief     proprietary transport layer packet algorithm implementation
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

#ifndef __PPT_TRANS_PKT_ALGO_H__
#define __PPT_TRANS_PKT_ALGO_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
*                              Header Files
*============================================================================*/
#include <stdint.h>
#include "app_section.h"
#include "ppt_trans_handle.h"


static inline T_PPT_TRANS_MOTION_DATA_SIZE ppt_trans_pkt_algo_get_motion_size(
    uint16_t data) RAM_FUNCTION;
static inline uint8_t ppt_trans_pkt_algo_get_seq_num(uint8_t *data_src, uint8_t len) RAM_FUNCTION;
/*============================================================================*
 *                             Static inline function
 *============================================================================*/
/******************************************************************
 * @brief  calculate how many bits is needed to represent given data
 * @param  data - optical data
 * @retval bits needed in pre-define enumeration
 */
static inline T_PPT_TRANS_MOTION_DATA_SIZE ppt_trans_pkt_algo_get_motion_size(uint16_t data)
{
    if (data == 0)
    {
        return PPT_TRANS_MOTION_SIZE_0BITS;
    }
    else if ((uint16_t)(data + 2) < 0x04)
    {
        return PPT_TRANS_MOTION_SIZE_2BITS;
    }
    else if ((uint16_t)(data + 8) < 0x10)
    {
        return PPT_TRANS_MOTION_SIZE_4BITS;
    }
    else if ((uint16_t)(data + 128) < 0x100)
    {
        return PPT_TRANS_MOTION_SIZE_8BITS;
    }
    else
    {
        return PPT_TRANS_MOTION_SIZE_16BITS;
    }
}

/******************************************************************
 * @brief  parse the sequence number given packet and length
 * @param  data_src - buffer of the specified packet
 * @param  len - length of the buffer
 * @retval bits needed in pre-define enumeration
 */
static inline uint8_t ppt_trans_pkt_algo_get_seq_num(uint8_t *data_src, uint8_t len)
{
    if ((len * 8) < (PPT_PKT_PAYLOAD_SIZE_HEADER + 1))
    {
        return 0;
    }
    return (data_src[1] >> 2);
}

/*============================================================================*
*                              Functions
*============================================================================*/
/******************************************************************
 * @brief  calculate the length of mouse data without long packet
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @retval length_out
 */
uint8_t ppt_trans_pkt_algo_calc_pkt_len(uint8_t *data_src, uint16_t length_src);

/******************************************************************
 * @brief  get latest trajectory datum from packet
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @param  data_out - latest mouse data
 * @retval void
 */
void ppt_trans_pkt_algo_get_latest_motion(uint8_t *data_src, uint16_t length_src,
                                          T_PPT_TRANS_MOUSE_DATA *data_out);

/******************************************************************
 * @brief  compose trajectory data to create a mouse packet
 * @param  data_src - trajectory datum to be composed
 * @param  length_src - indicates how many trajectory points are passed in
 * @param  data_out - raw packet data to be transmit
 * @param  length_out - indicates how many bytes are to be sent
 * @param  seq_num - sequence number of this packet
 * @retval void
 */
void ppt_trans_pkt_algo_compose_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                    uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);

/******************************************************************
 * @brief  compose one trajectory data and compensation data to create a fixed-length
 * @param  data_src - trajectory datum to be composed
 * @param  length_src - indicates how many trajectory points are passed in
 * @param  data_pos_in - byte array indicating compensation data
 * @param  len_pos - length of `data_pos_in` in bytes
 * @param  data_out - raw packet data to be transmit
 * @param  length_out - indicates how many bytes are to be sent
 * @param  seq_num - sequence number of this packet
 * @retval void
 */
void ppt_trans_pkt_algo_compose_pos_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                        uint8_t *data_pos_in, uint8_t len_pos,
                                        uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);

/******************************************************************
 * @brief  compose trajectory points and compensation data to create a mouse packet
 * @param  data_src - trajectory datum to be composed
 * @param  length_src - indicates how many trajectory points are passed in
 * @param  data_pos_in - byte array indicating compensation data
 * @param  len_pos - length of `data_pos_in` in bytes
 * @param  data_out - raw packet data to be transmit
 * @param  length_out - indicates how many bytes are to be sent
 * @param  seq_num - sequence number of this packet
 * @retval void
 */
void ppt_trans_pkt_algo_compose_enh_pos_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                            uint8_t *data_pos_in, uint8_t len_pos,
                                            uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);

/******************************************************************
 * @brief  decompose mouse packet to trajectory datum
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @param  data_out - trajectory datum to be stored
 * @param  length_out - indicates how many trajectory datun are parsed
 * @param  seq_num - sequence number of this packet
 * @param  used_bytes - used bytes to parse mouse data
 * @retval void
 */
void ppt_trans_pkt_algo_parse_pkt(uint8_t *data_src, uint16_t length_src,
                                  T_PPT_TRANS_MOUSE_DATA *data_out, uint8_t *length_out,
                                  uint8_t *seq_num, uint8_t *used_bytes);

/******************************************************************
 * @brief  decompose fixed-length mouse packet to compensation data and trajectory datum
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @param  pos_out - compensation datum to be stored
 * @param  len_pos - length of `pos_out` in bytes
 * @param  but_out - trajectory data including cursor offset/button/wheel
 * @param  length_out - indicates how many trajectory datun are parsed
 * @param  seq_num - sequence number of this packet
 * @retval void
 */
void ppt_trans_pkt_algo_parse_pos_pkt(uint8_t *data_src, uint16_t length_src,
                                      uint8_t *pos_out, uint16_t length_pos,
                                      T_PPT_TRANS_MOUSE_DATA *data_out, uint8_t *length_out,
                                      uint8_t *seq_num);

/******************************************************************
 * @brief  decompose mouse packet to compensation data and trajectory datum
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @param  pos_out - compensation datum to be stored
 * @param  len_pos - length of `pos_out` in bytes
 * @param  but_out - trajectory data including cursor offset/button/wheel
 * @param  length_out - indicates how many trajectory datun are parsed
 * @param  seq_num - sequence number of this packet
 * @retval void
 */
void ppt_trans_pkt_algo_parse_enh_pos_pkt(uint8_t *data_src, uint16_t length_src,
                                          uint8_t *pos_out, uint16_t length_pos,
                                          T_PPT_TRANS_MOUSE_DATA *but_out, uint8_t *length_out,
                                          uint8_t *seq_num);

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_PKT_ALGO_H__
