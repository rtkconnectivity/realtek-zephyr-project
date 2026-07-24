/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_pkt_algo.c
   * @brief     mouse packet algorithm implementation
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "board.h"
#include "trace.h"
#include "string.h"
#include "app_section.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_pkt_algo.h"

/*============================================================================*
 *                              Defines
 *============================================================================*/
#define BITFIELD_MASK(width)     (BIT(width) - 1)
#define VAR_MAX(element)         (BITFIELD_MASK(sizeof(element) * 8))

/*============================================================================*
 *                              Static Variables
 *============================================================================*/
/** previous composed button data */
static uint8_t prev_button = 0;
/** previous composed wheel data
 *  @ref T_PPT_TRANS_WHEEL_DIRECTION
*/
static uint8_t prev_wheel = 0;

/*============================================================================*
 *                              Function Declarations
 *============================================================================*/
void ppt_trans_pkt_algo_compose_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                    uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);
void ppt_trans_pkt_algo_parse_pkt(uint8_t *data_src, uint16_t length_src,
                                  T_PPT_TRANS_MOUSE_DATA *data_out, uint8_t *length_out,
                                  uint8_t *seq_num, uint8_t *used_bytes);
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
void ppt_trans_pkt_algo_compose_pos_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                        uint8_t *data_pos_in, uint8_t len_pos,
                                        uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);
void ppt_trans_pkt_algo_parse_pos_pkt(uint8_t *data_src, uint16_t length_src,
                                      uint8_t *pos_out, uint16_t length_pos,
                                      T_PPT_TRANS_MOUSE_DATA *but_out, uint8_t *length_out,
                                      uint8_t *seq_num);
void ppt_trans_pkt_algo_compose_enh_pos_pkt(T_PPT_TRANS_MOUSE_DATA *data_src, uint8_t length_src,
                                            uint8_t *data_pos_in, uint8_t len_pos,
                                            uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);
void ppt_trans_pkt_algo_parse_enh_pos_pkt(uint8_t *data_src, uint16_t length_src,
                                          uint8_t *pos_out, uint16_t length_pos,
                                          T_PPT_TRANS_MOUSE_DATA *but_out, uint8_t *length_out,
                                          uint8_t *seq_num);
#endif
uint8_t ppt_trans_pkt_algo_calc_pkt_len(uint8_t *data_src, uint16_t length_src);
void ppt_trans_pkt_algo_get_latest_motion(uint8_t *data_src, uint16_t length_src,
                                          T_PPT_TRANS_MOUSE_DATA *data_out);
static void read_bitfield_from_pkt(uint8_t *bits_offset, uint8_t *p_data, uint8_t *output,
                                   uint8_t rd_width);
static void extend_motion_data_to_u16(uint16_t *value, uint8_t original_size);
/*============================================================================*
 *                              Static Functions
 *============================================================================*/
/******************************************************************
 * @brief  read spefific bitfield from transport ppt data packet
 * @param  bits_offset - the offset in bits to write bitfield
 * @param  p_data - pointer of transport ppt data packet
 * @param  output - indicates the read data
 * @param  rd_width - indicates read length in bits
 * @retval void
 */
static void read_bitfield_from_pkt(uint8_t *bits_offset, uint8_t *p_data, uint8_t *output,
                                   uint8_t rd_width)
{
    uint8_t sizeof_byte = 8;
    uint8_t offset_byte = *bits_offset / sizeof_byte;
    uint8_t offset = *bits_offset % sizeof_byte;
    uint8_t *p_buff = p_data + offset_byte;

    uint8_t delta_num = offset + rd_width;
    *output = 0;

    if (delta_num <= sizeof_byte)
    {
        *output = *p_buff >> (sizeof_byte - delta_num);
    }
    else
    {
        uint8_t bit_left = sizeof_byte - offset;
        uint8_t bit_cross = delta_num - sizeof_byte;
        *output = (*p_buff & BITFIELD_MASK(bit_left)) << bit_cross;
        p_buff++;
        *output |= (*p_buff >> (sizeof_byte - bit_cross));
    }
    *output &= BITFIELD_MASK(rd_width);
    *bits_offset += rd_width;
}

/******************************************************************
 * @brief  extend the motion value from 2/4/8 bits to 16bit
 * @param  motion_data - the value to be extended to 16bit
 * @param  original_size - received size of the motion data
 * @retval void
 */
static void extend_motion_data_to_u16(uint16_t *value, uint8_t original_size)
{
    // check if sign bit is 1
    if (BIT(original_size - 1) & (*value))
    {
        *value |= (VAR_MAX(*value) & ~BITFIELD_MASK(original_size));
    }
}

/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  calculate the length of mouse data without long packet
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @retval length_out - the length of mouse data in unit bytes
 */
uint8_t ppt_trans_pkt_algo_calc_pkt_len(uint8_t *data_src, uint16_t length_src)
{
    if (length_src == 0)
    {
        return 0;
    }

    uint8_t offset = 0;
    uint8_t motion_size;

    T_PPT_TRANS_PKT_HEADER header =
    {
        .d8 = data_src[0]
    };

    offset = PPT_PKT_PAYLOAD_SIZE_HEADER;

    if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
    {
        offset += PPT_PKT_PAYLOAD_SIZE_SEQ;
    }

    if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    {
        offset += PPT_PKT_PAYLOAD_SIZE_BUTTON;
    }

    if (header.wheel_en == PPT_TRANS_FIELD_ENABLE)
    {
        offset += PPT_PKT_PAYLOAD_SIZE_WHEEL;
    }

    if (header.motion_size != PPT_TRANS_MOTION_SIZE_0BITS)
    {
        motion_size = (1 << header.motion_size);
        offset += (PPT_PKT_PAYLOAD_DATA_NUM_MAX * 2 * motion_size);
    }

    return (offset + 7) / 8;
}

/******************************************************************
 * @brief  get latest trajectory datum from packet
 * @param  data_src - raw packet data to be parsed
 * @param  length_src - indicates how many bytes are passed in
 * @param  data_out - latest mouse data
 * @retval void
 */
void ppt_trans_pkt_algo_get_latest_motion(uint8_t *data_src, uint16_t length_src,
                                          T_PPT_TRANS_MOUSE_DATA *data_out)
{
    if (length_src < 2)
    {
        return;
    }
    uint8_t offset = 0;
    uint8_t motion_size;
    uint16_t motion_x = 0;
    uint16_t motion_y = 0;

    T_PPT_TRANS_PKT_HEADER header =
    {
        .d8 = data_src[0]
    };
    offset = PPT_PKT_PAYLOAD_SIZE_HEADER;
    if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
    {
        offset += PPT_PKT_PAYLOAD_SIZE_SEQ;
    }

    if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &(data_out->button), PPT_PKT_PAYLOAD_SIZE_BUTTON);
    }
    else
    {
        data_out->button = 0;
    }

    if (header.wheel_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &(data_out->wheel_direction), PPT_PKT_PAYLOAD_SIZE_WHEEL);
    }
    else
    {
        data_out->wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF;
    }

    if (header.motion_size == PPT_TRANS_MOTION_SIZE_0BITS)
    {
        data_out->optical_x = 0;
        data_out->optical_y = 0;
        return;
    }
    motion_size = (1 << header.motion_size);
    offset += ((PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1) * 2 * motion_size);

    if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
    {
        uint8_t temp_x = 0, temp_y = 0;
        read_bitfield_from_pkt(&offset, data_src, &temp_x, 8);
        motion_x = (uint16_t)temp_x << 8;
        read_bitfield_from_pkt(&offset, data_src, &temp_x, motion_size - 8);
        motion_x |= temp_x;
        read_bitfield_from_pkt(&offset, data_src, &temp_y, 8);
        motion_y = (uint16_t)temp_y << 8;
        read_bitfield_from_pkt(&offset, data_src, &temp_y, motion_size - 8);
        motion_y |= temp_y;
    }
    else
    {
        read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&motion_x, motion_size);
        read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&motion_y, motion_size);
    }
    extend_motion_data_to_u16(&motion_x, motion_size);
    extend_motion_data_to_u16(&motion_y, motion_size);
    data_out->optical_x = (int16_t)motion_x;
    data_out->optical_y = (int16_t)motion_y;

    return;
}

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
                                  uint8_t *seq_num, uint8_t *used_bytes)
{
    if (length_src == 0)
    {
        *length_out = 0;
        *seq_num = 0xFF;
        *used_bytes = length_src;
        return;
    }
    T_PPT_TRANS_PKT_HEADER header;
    T_PPT_TRANS_MOUSE_DATA *payload = data_out;
    header.d8 = data_src[0];

    uint8_t temp_wheel = PPT_TRANS_WHEEL_RELEASE_DEF;
    uint8_t temp_button = 0;
    uint8_t payload_idx = 0;
    uint8_t offset = PPT_PKT_PAYLOAD_SIZE_HEADER;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO1("[ppt_trans_pkt_algo_parse_pkt] before seq, offset: %d", offset);
#endif
    if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, seq_num, PPT_PKT_PAYLOAD_SIZE_SEQ);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before button, offset: %d, seq: %d", offset,
                    *seq_num);
#endif
    if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &temp_button, PPT_PKT_PAYLOAD_SIZE_BUTTON);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before wheel, offset: %d, button: %d", offset,
                    temp_button);
#endif
    if (header.wheel_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &temp_wheel, PPT_PKT_PAYLOAD_SIZE_WHEEL);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before motion, offset: %d, wheel: %d", offset,
                    temp_wheel);
#endif
    if (header.motion_size != PPT_TRANS_MOTION_SIZE_0BITS)
    {
        uint8_t motion_size = (1 << header.motion_size);
        while (((offset + motion_size) <= length_src * 8) &&
               (payload_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX))
        {
            if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
            {
                uint8_t temp_x = 0, temp_y = 0;
                read_bitfield_from_pkt(&offset, data_src, &temp_x, 8);
                payload[payload_idx].optical_x = (uint16_t)temp_x << 8;
                read_bitfield_from_pkt(&offset, data_src, &temp_x, motion_size - 8);
                payload[payload_idx].optical_x |= temp_x;
                read_bitfield_from_pkt(&offset, data_src, &temp_y, 8);
                payload[payload_idx].optical_y = (uint16_t)temp_y << 8;
                read_bitfield_from_pkt(&offset, data_src, &temp_y, motion_size - 8);
                payload[payload_idx].optical_y |= temp_y;
            }
            else
            {
                read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&payload[payload_idx].optical_x, motion_size);
                read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&payload[payload_idx].optical_y, motion_size);
            }
            extend_motion_data_to_u16((uint16_t *)&payload[payload_idx].optical_x, motion_size);
            extend_motion_data_to_u16((uint16_t *)&payload[payload_idx].optical_y, motion_size);

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
            APP_PRINT_INFO5("[ppt_trans_pkt_algo_parse_pkt] after read motion %d, size %d, offset: %d, x,y: %d %d",
                            payload_idx, motion_size, offset, payload[payload_idx].optical_x, payload[payload_idx].optical_y);
#endif
            payload_idx++;
        }
    }
    else
    {
        while (payload_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX)
        {
            memset(payload + payload_idx, 0, sizeof(T_PPT_TRANS_MOUSE_DATA));
            payload[payload_idx].wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF;
            payload_idx ++;
        }
    }

    payload[PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1].button = temp_button;
    payload[PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1].wheel_direction = temp_wheel;

    *length_out = payload_idx;
    /** unconditional round up */
    *used_bytes = (offset + 7) / 8;
}


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
                                    uint8_t *data_out, uint8_t *length_out, uint8_t seq_num)
{
    /** dynamic length packet format:
     *  header | seq | button | wheel |  x   |  y  |
     *    8b     6b      7b      3b     N*2    N*2
     *  N = 2 or 4 or 8 or 16bits, depending on the size of largest motion data
     *  each packet will carry 2 trajectory records
    */
    T_PPT_TRANS_PKT_HEADER header;
    uint8_t packet_len = 0;
    header.d8 = 0;
    header.seq_en = PPT_TRANS_FIELD_ENABLE;
    header.pkt_type = PPT_TRANS_PKT_TYPE_OFFSET & (BIT(PPT_PKT_HDR_SIZE_PKT_TYPE) - 1);
    if ((prev_button == data_src[length_src - 1].button) &&
        (prev_button == 0x00))
    {
        header.button_en = PPT_TRANS_FIELD_DISABLE;
    }
    else
    {
        header.button_en = PPT_TRANS_FIELD_ENABLE;
    }

    uint8_t wheel_data = data_src[length_src - 1].wheel_direction;
    if ((prev_wheel == wheel_data) &&
        (prev_wheel == PPT_TRANS_WHEEL_RELEASE_DEF))
    {
        header.wheel_en = PPT_TRANS_FIELD_DISABLE;
    }
    else
    {
        header.wheel_en = PPT_TRANS_FIELD_ENABLE;
    }

    uint8_t motion_size = PPT_TRANS_MOTION_SIZE_0BITS;
    uint8_t temp_size_x, temp_size_y, temp_size;
    for (uint8_t i = 0; i < length_src; i++)
    {
        temp_size_x = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_x);
        temp_size_y = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_y);

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
        APP_PRINT_INFO4("[ppt_trans_pkt_algo_compose_pkt]original: 0x%x, 0x%x, size:%d, %d",
                        data_src[i].optical_x, data_src[i].optical_y, temp_size_x, temp_size_y);
#endif

        temp_size = (temp_size_x > temp_size_y) ? temp_size_x : temp_size_y;
        motion_size = (temp_size > motion_size) ? temp_size : motion_size;
    }
    header.motion_size = motion_size;

    data_out[packet_len++] = header.d8;
    data_out[packet_len] = (seq_num << 2);
    uint8_t left_bits = 2;
    if (header.button_en == 1)
    {
        uint8_t button_data = data_src[length_src - 1].button;
        data_out[packet_len++] |= ((button_data >> 5) & 0x3);
        data_out[packet_len] = (button_data & 0x1f) << 3;
        left_bits = 3;
    }

    if (header.wheel_en == 1)
    {
        if (header.button_en == 1)
        {
            data_out[packet_len++] |= (wheel_data & 0x7);
            data_out[packet_len] = 0;
            left_bits = 8;
        }
        else
        {
            data_out[packet_len++] |= ((wheel_data >> 1) & 0x3);
            data_out[packet_len] = (wheel_data & 0x1) << 7;
            left_bits = 7;
        }
    }

    if (header.motion_size != PPT_TRANS_MOTION_SIZE_0BITS)
    {
        uint8_t i = 0;
        uint8_t byte_size = 8;
        /** buffer of X and Y data, will need 2 * DATA_NUM_MAX * sizeof(X/Y) */
        uint16_t temp_motion_data[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV * 2] = {0};
        uint16_t motion_clr_mask;
        motion_size = 1 << header.motion_size;
        motion_clr_mask = (1 << motion_size) - 1;
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_pkt_algo_compose_pkt] size: %d, mask: 0x%x", motion_size,
                        motion_clr_mask);
#endif
        for (i = 0; i < length_src; i++)
        {
            temp_motion_data[2 * i] = data_src[i].optical_x & motion_clr_mask;
            temp_motion_data[2 * i + 1] = data_src[i].optical_y & motion_clr_mask;
        }
        for (i = 0; i < length_src * 2; i++)
        {
            if (left_bits <= motion_size)
            {
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
                if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
                {
                    APP_PRINT_INFO4("[ppt_trans_pkt_algo_compose_pkt] before: 0x%x 0x%x 0x%x, left_bits: %d",
                                    data_out[packet_len], data_out[packet_len + 1], data_out[packet_len + 2], left_bits);
                }
                else
                {
                    APP_PRINT_INFO2("[ppt_trans_pkt_algo_compose_pkt] before: 0x%x, 0x%x",
                                    data_out[packet_len], data_out[packet_len + 1]);
                }
#endif
                /** pack to last bit of current byte */
                data_out[packet_len++] |= ((temp_motion_data[i] >> (motion_size - left_bits)));
                if (motion_size - left_bits > byte_size)
                {
                    data_out[packet_len++] = temp_motion_data[i] >> (motion_size - left_bits - byte_size);
                    byte_size += byte_size;
                }
                /** go to next byte and pack left data bits */
                data_out[packet_len] = (temp_motion_data[i] << (byte_size + left_bits - motion_size));
                left_bits = byte_size + left_bits - motion_size;
                byte_size = 8;
#if PPT_PKT_ALGO_DBG_LOG_EN
                if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
                {
                    APP_PRINT_INFO5("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x 0x%x 0x%x, left_bits: %d, data: 0x%x",
                                    data_out[packet_len - 2], data_out[packet_len - 1], data_out[packet_len],
                                    left_bits, temp_motion_data[i]);
                }
                else
                {
                    APP_PRINT_INFO4("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x 0x%x, left_bits: %d, data: 0x%x",
                                    data_out[packet_len - 1], data_out[packet_len], left_bits, temp_motion_data[i]);
                }
#endif
            }
            else
            {
                if (left_bits == byte_size)
                {
                    data_out[packet_len] = (temp_motion_data[i] << (left_bits - motion_size));
                }
                else
                {
                    data_out[packet_len] |= (temp_motion_data[i] << (left_bits - motion_size));
                }
                left_bits -= motion_size;
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
                APP_PRINT_INFO3("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x, left_bits: %d data: 0x%x",
                                data_out[packet_len], left_bits, temp_motion_data[i]);
#endif
            }
        }
    }
    *length_out = (left_bits == 8) ? packet_len : (packet_len + 1);
    prev_button = data_src[length_src - 1].button;
    prev_wheel = wheel_data;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO3("[ppt_trans_pkt_algo_compose_pkt] left_bits: %d, prev_button: %d, prev_wheel: %d",
                    left_bits, prev_button, prev_wheel);
#endif
    return;
}

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
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
                                        uint8_t *data_out, uint8_t *length_out, uint8_t seq_num)
{
    /**
     * header contains MS 5bit of latest optical_x, wheel data is replaced by LS 3bit of latest optical_x
     * last byte is latest optical_y, disable wheel, long packet, always send button
    */
    T_PPT_TRANS_PKT_HEADER header;
    uint8_t packet_len = 0;
    header.d8 = 0;
    header.seq_en = PPT_TRANS_FIELD_ENABLE;
    header.pkt_type = PPT_TRANS_PKT_TYPE_COMPENSATE & (BIT(PPT_PKT_HDR_SIZE_PKT_TYPE) - 1);

    data_out[packet_len] = header.d8;
    uint8_t motion_size_x = PPT_TRANS_MOTION_SIZE_0BITS;
    uint8_t motion_size_y = PPT_TRANS_MOTION_SIZE_0BITS;
    motion_size_x = ppt_trans_pkt_algo_get_motion_size((uint16_t)data_src[length_src - 1].optical_x);
    motion_size_y = ppt_trans_pkt_algo_get_motion_size((uint16_t)data_src[length_src - 1].optical_y);
    if ((motion_size_x > PPT_TRANS_MOTION_SIZE_4BITS) ||
        (motion_size_y > PPT_TRANS_MOTION_SIZE_4BITS))
    {
        data_src[length_src - 1].optical_x = 0;
        data_src[length_src - 1].optical_y = 0;
    }

    /** 4bit for x */
    data_out[packet_len] |= (data_src[length_src - 1].optical_x << 1);
    /** 1bit for y */
    data_out[packet_len] |= ((data_src[length_src - 1].optical_y >> 3) & 0x01);

    data_out[++packet_len] = (seq_num << 2);
    uint8_t left_bits = 2;

    /** always send button */
    uint8_t button_data = data_src[length_src - 1].button;
    data_out[packet_len++] |= ((button_data >> 5) & 0x3);
    data_out[packet_len] = (button_data & 0x1f) << 3;
    left_bits = 3;

    /** 3bit for y */
    data_out[packet_len++] |= (data_src[length_src - 1].optical_y & 0x07);
    left_bits = 8;


    uint8_t used_bits = 0;
    uint8_t total_bits = 8 * len_pos;
    for (uint8_t i = 0; i < len_pos; i++)
    {
        if (left_bits == 8)
        {
            data_out[packet_len] = 0;
        }
        data_out[packet_len++] |= ((data_pos_in[i] >> (8 - left_bits)) & BIT(left_bits) - 1);
        data_out[packet_len] = (data_pos_in[i] << (left_bits));
    }

    *length_out = packet_len;
    prev_button = data_src[length_src - 1].button;
    prev_wheel = PPT_TRANS_WHEEL_RELEASE_DEF;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO3("[ppt_trans_pkt_algo_compose_pos_pkt] left_bits: %d, prev_button: %d, prev_wheel: %d",
                    left_bits, prev_button, prev_wheel);
#endif
    return;
}

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
                                      T_PPT_TRANS_MOUSE_DATA *but_out, uint8_t *length_out,
                                      uint8_t *seq_num)
{
    if (length_src == 0)
    {
        *length_out = 0;
        *seq_num = 0xFF;
        return;
    }
    T_PPT_TRANS_PKT_HEADER header;
    T_PPT_TRANS_MOUSE_DATA *payload = but_out;
    header.d8 = data_src[0];

    const uint8_t fixed_trajectory_coun = 1;
    uint8_t temp_wheel = PPT_TRANS_WHEEL_RELEASE_DEF;
    uint8_t temp_button = 0;
    uint8_t optical_x = 0;
    uint8_t optical_y_upper = 0;
    uint8_t optical_y_lower = 0;
    uint8_t offset = 3;

    /** read upper x from header */
    read_bitfield_from_pkt(&offset, data_src, &optical_x, 4);
    read_bitfield_from_pkt(&offset, data_src, &optical_y_upper, 1);

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO1("[ppt_trans_pkt_algo_parse_pkt] before seq, offset: %d", offset);
#endif
    if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, seq_num, PPT_PKT_PAYLOAD_SIZE_SEQ);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before button, offset: %d, seq: %d", offset,
                    *seq_num);
#endif
    // if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    if (true) // always send button
    {
        read_bitfield_from_pkt(&offset, data_src, &temp_button, PPT_PKT_PAYLOAD_SIZE_BUTTON);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before wheel, offset: %d, button: %d", offset,
                    temp_button);
#endif

    read_bitfield_from_pkt(&offset, data_src, &optical_y_lower, 3);

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before motion, offset: %d, wheel: %d", offset,
                    temp_wheel);
#endif
    if (true)
    {
        uint8_t motion_size = 8;
        uint8_t cnt = 0;
        while (offset < length_src * 8)
        {
            read_bitfield_from_pkt(&offset, data_src, pos_out + cnt, motion_size);
            cnt++;
        }
    }

    payload[fixed_trajectory_coun - 1].optical_x = optical_x;
    payload[fixed_trajectory_coun - 1].optical_y = (optical_y_upper << 3) | optical_y_lower;
    extend_motion_data_to_u16((uint16_t *)&payload[fixed_trajectory_coun - 1].optical_x, 4);
    extend_motion_data_to_u16((uint16_t *)&payload[fixed_trajectory_coun - 1].optical_y, 4);

    payload[fixed_trajectory_coun - 1].button = temp_button;
    payload[fixed_trajectory_coun - 1].wheel_direction = temp_wheel;

    *length_out = fixed_trajectory_coun;
}

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
                                            uint8_t *data_out, uint8_t *length_out, uint8_t seq_num)
{
    /** dynamic length packet format:
     *  header | seq | button | wheel | pos |  x   |  y  |
     *    8b     6b      7b      3b      M    N*2    N*2
     *  M = len_pos bytes
     *  N = 2 or 4 or 8 or 16bits, depending on the size of largest motion data
     *  each packet will carry multi trajectory records
    */
    T_PPT_TRANS_PKT_HEADER header;
    uint8_t packet_len = 0;
    header.d8 = 0;
    header.seq_en = PPT_TRANS_FIELD_ENABLE;
    header.pkt_type = PPT_TRANS_PKT_TYPE_COMPENSATE & (BIT(PPT_PKT_HDR_SIZE_PKT_TYPE) - 1);
    if ((prev_button == data_src[length_src - 1].button) &&
        (prev_button == 0x00))
    {
        header.button_en = PPT_TRANS_FIELD_DISABLE;
    }
    else
    {
        header.button_en = PPT_TRANS_FIELD_ENABLE;
    }

    uint8_t wheel_data = data_src[length_src - 1].wheel_direction;
    if ((prev_wheel == wheel_data) &&
        (prev_wheel == PPT_TRANS_WHEEL_RELEASE_DEF))
    {
        header.wheel_en = PPT_TRANS_FIELD_DISABLE;
    }
    else
    {
        header.wheel_en = PPT_TRANS_FIELD_ENABLE;
    }

    /** find max motion data bits */
    uint8_t motion_size = PPT_TRANS_MOTION_SIZE_0BITS;
    uint8_t temp_size_x, temp_size_y, temp_size;
    uint8_t motion_size_1st = PPT_TRANS_MOTION_SIZE_0BITS;
    uint8_t i = 0;
    temp_size_x = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_x);
    temp_size_y = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_y);
    motion_size_1st = (temp_size_x > temp_size_y) ? temp_size_x : temp_size_y;
    for (i = 1; i < length_src; i++)
    {
        temp_size_x = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_x);
        temp_size_y = ppt_trans_pkt_algo_get_motion_size(data_src[i].optical_y);
        temp_size = (temp_size_x > temp_size_y) ? temp_size_x : temp_size_y;
        motion_size = (temp_size > motion_size) ? temp_size : motion_size;
    }
    /** check is exceeding max pkt size and need reduce one motion point. */
    if (header.button_en == PPT_TRANS_FIELD_ENABLE
        && ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent < 13
        && length_src > 2
        && (motion_size == PPT_TRANS_MOTION_SIZE_8BITS || motion_size_1st == PPT_TRANS_MOTION_SIZE_8BITS)
       )
    {
        /** reduce motion point due to exceed pkt size */
        length_src--;
        data_src++;
    }
    else
    {
        motion_size = (motion_size_1st > motion_size) ? motion_size_1st : motion_size;
    }
    header.motion_size = motion_size;
    data_out[packet_len] = header.d8;

    /** append sequence number */
    data_out[++packet_len] = (seq_num << 2);
    uint8_t left_bits = 2;

    /** append send button if enabled */
    if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    {
        uint8_t button_data = data_src[length_src - 1].button;
        data_out[packet_len++] |= ((button_data >> 5) & 0x3);
        data_out[packet_len] = (button_data & 0x1f) << 3;
        left_bits = 3;
    }

    /** append wheel if enabled */
    if (header.wheel_en == PPT_TRANS_FIELD_ENABLE)
    {
        if (header.button_en == PPT_TRANS_FIELD_ENABLE)
        {
            data_out[packet_len++] |= (wheel_data & 0x7);
            data_out[packet_len] = 0;
            left_bits = 8;
        }
        else
        {
            data_out[packet_len++] |= ((wheel_data >> 1) & 0x3);
            data_out[packet_len] = (wheel_data & 0x1) << 7;
            left_bits = 7;
        }
    }

    /** byte alignment for pos and offset data */
    if (left_bits != 8)
    {
        packet_len++;
        left_bits = 8;
    }

    /** append pos data */
    uint8_t used_bits = 0;
    uint8_t total_bits = 8 * len_pos;
    for (uint8_t i = 0; i < len_pos; i++)
    {
        if (left_bits == 8)
        {
            data_out[packet_len] = 0;
        }
        data_out[packet_len++] |= ((data_pos_in[i] >> (8 - left_bits)) & BIT(left_bits) - 1);
        data_out[packet_len] = (data_pos_in[i] << (left_bits));
    }

    /** append offset data */
    if (header.motion_size != PPT_TRANS_MOTION_SIZE_0BITS)
    {
        uint8_t i = 0;
        uint8_t byte_size = 8;
        uint16_t temp_motion_data[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
        uint16_t motion_clr_mask;
        motion_size = 1 << header.motion_size;
        motion_clr_mask = (1 << motion_size) - 1;
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_pkt_algo_compose_pkt] size: %d, mask: 0x%x", motion_size,
                        motion_clr_mask);
#endif
        for (i = 0; i < length_src; i++)
        {
            temp_motion_data[2 * i] = data_src[i].optical_x & motion_clr_mask;
            temp_motion_data[2 * i + 1] = data_src[i].optical_y & motion_clr_mask;
        }
        for (i = 0; i < length_src * 2; i++)
        {
            if (left_bits <= motion_size)
            {
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
                if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
                {
                    APP_PRINT_INFO4("[ppt_trans_pkt_algo_compose_pkt] before: 0x%x 0x%x 0x%x, left_bits: %d",
                                    data_out[packet_len], data_out[packet_len + 1], data_out[packet_len + 2], left_bits);
                }
                else
                {
                    APP_PRINT_INFO2("[ppt_trans_pkt_algo_compose_pkt] before: 0x%x, 0x%x",
                                    data_out[packet_len], data_out[packet_len + 1]);
                }
#endif
                /** pack to last bit of current byte */
                data_out[packet_len++] |= ((temp_motion_data[i] >> (motion_size - left_bits)));
                if (motion_size - left_bits > byte_size)
                {
                    data_out[packet_len++] = temp_motion_data[i] >> (motion_size - left_bits - byte_size);
                    byte_size += byte_size;
                }
                /** go to next byte and pack left data bits */
                data_out[packet_len] = (temp_motion_data[i] << (byte_size + left_bits - motion_size));
                left_bits = byte_size + left_bits - motion_size;
                byte_size = 8;
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
                if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
                {
                    APP_PRINT_INFO5("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x 0x%x 0x%x, left_bits: %d, data: 0x%x",
                                    data_out[packet_len - 2], data_out[packet_len - 1], data_out[packet_len],
                                    left_bits, temp_motion_data[i]);
                }
                else
                {
                    APP_PRINT_INFO4("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x 0x%x, left_bits: %d, data: 0x%x",
                                    data_out[packet_len - 1], data_out[packet_len], left_bits, temp_motion_data[i]);
                }
#endif
            }
            else
            {
                if (left_bits == byte_size)
                {
                    data_out[packet_len] = (temp_motion_data[i] << (left_bits - motion_size));
                }
                else
                {
                    data_out[packet_len] |= (temp_motion_data[i] << (left_bits - motion_size));
                }
                left_bits -= motion_size;
#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
                APP_PRINT_INFO3("[ppt_trans_pkt_algo_compose_pkt] payload: 0x%x, left_bits: %d data: 0x%x",
                                data_out[packet_len], left_bits, temp_motion_data[i]);
#endif
            }
        }
    }

    *length_out = (left_bits == 8) ? packet_len : (packet_len + 1);
    prev_button = data_src[length_src - 1].button;
    prev_wheel = wheel_data;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO3("[ppt_trans_pkt_algo_compose_pos_pkt] left_bits: %d, prev_button: %d, prev_wheel: %d",
                    left_bits, prev_button, prev_wheel);
#endif
    return;
}

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
                                          uint8_t *seq_num)
{
    if (length_src == 0)
    {
        *length_out = 0;
        *seq_num = 0xFF;
        return;
    }
    T_PPT_TRANS_PKT_HEADER header;
    T_PPT_TRANS_MOUSE_DATA *payload = but_out;
    header.d8 = data_src[0];

    uint8_t temp_wheel = PPT_TRANS_WHEEL_RELEASE_DEF;
    uint8_t temp_button = 0;
    uint8_t optical_x = 0;
    uint8_t optical_y_upper = 0;
    uint8_t optical_y_lower = 0;
    uint8_t offset = 8;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO1("[ppt_trans_pkt_algo_parse_pkt] before seq, offset: %d", offset);
#endif
    if (header.seq_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, seq_num, PPT_PKT_PAYLOAD_SIZE_SEQ);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before button, offset: %d, seq: %d", offset,
                    *seq_num);
#endif
    if (header.button_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &temp_button, PPT_PKT_PAYLOAD_SIZE_BUTTON);
    }

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before wheel, offset: %d, button: %d", offset,
                    temp_button);
#endif
    if (header.wheel_en == PPT_TRANS_FIELD_ENABLE)
    {
        read_bitfield_from_pkt(&offset, data_src, &temp_wheel, PPT_PKT_PAYLOAD_SIZE_WHEEL);
    }

    /** byte alignment for pos and offset data */
    offset = (offset + 7) & 0xF8;

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_pkt_algo_parse_pkt] before motion, offset: %d, wheel: %d", offset,
                    temp_wheel);
#endif

    /** read pos data */
    if (true)
    {
        uint8_t motion_size = 8;
        uint8_t cnt = 0;
        while (cnt < length_pos)
        {
            read_bitfield_from_pkt(&offset, data_src, pos_out + cnt, motion_size);
            cnt++;
        }
    }

    /** read offset data */
    uint8_t payload_idx = 0;
    if (header.motion_size != PPT_TRANS_MOTION_SIZE_0BITS)
    {
        uint8_t motion_size = (1 << header.motion_size);
        while (((offset + motion_size) <= length_src * 8) &&
               (payload_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1))
        {
            if (header.motion_size == PPT_TRANS_MOTION_SIZE_16BITS)
            {
                uint8_t temp_x = 0, temp_y = 0;
                read_bitfield_from_pkt(&offset, data_src, &temp_x, 8);
                payload[payload_idx].optical_x = (uint16_t)temp_x << 8;
                read_bitfield_from_pkt(&offset, data_src, &temp_x, motion_size - 8);
                payload[payload_idx].optical_x |= temp_x;
                read_bitfield_from_pkt(&offset, data_src, &temp_y, 8);
                payload[payload_idx].optical_y = (uint16_t)temp_y << 8;
                read_bitfield_from_pkt(&offset, data_src, &temp_y, motion_size - 8);
                payload[payload_idx].optical_y |= temp_y;
            }
            else
            {
                read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&payload[payload_idx].optical_x, motion_size);
                read_bitfield_from_pkt(&offset, data_src, (uint8_t *)&payload[payload_idx].optical_y, motion_size);
            }
            extend_motion_data_to_u16((uint16_t *)&payload[payload_idx].optical_x, motion_size);
            extend_motion_data_to_u16((uint16_t *)&payload[payload_idx].optical_y, motion_size);

#if PPT_TRANS_PKT_ALGO_DBG_LOG_EN
            APP_PRINT_INFO5("[ppt_trans_pkt_algo_parse_pkt] after read motion %d, size %d, offset: %d, x,y: %d %d",
                            payload_idx, motion_size, offset, payload[payload_idx].optical_x, payload[payload_idx].optical_y);
#endif
            payload_idx++;
        }
    }
    else
    {
        while (payload_idx < PPT_PKT_PAYLOAD_DATA_NUM_MAX - 1)
        {
            memset(payload + payload_idx, 0, sizeof(T_PPT_TRANS_MOUSE_DATA));
            payload[payload_idx].wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF;
            payload_idx ++;
        }
    }
    *length_out = payload_idx;

    payload[*length_out - 1].button = temp_button;
    payload[*length_out - 1].wheel_direction = temp_wheel;
}

#endif // PPT_TRANS_FEATURE_SUPPORT_POS_CTRL

