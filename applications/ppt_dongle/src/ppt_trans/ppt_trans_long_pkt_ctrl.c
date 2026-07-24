/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_long_pkt_ctrl.c
   * @brief     long packet data control module
   * @author
   * @date
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
#include "stdint.h"
#include "string.h"
#include "trace.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_long_pkt_ctrl.h"
#include "app_section.h"
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
#include "ppt_trans_chann_ctrl.h"
#endif


#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
/*============================================================================*
 *                              Definitions
 *============================================================================*/
#define MIN(x,y)                      (x<y ? x:y)
#define CRC_LEN                       2
#define HEADER_LEN                    2
#define TOTAL_LEN                     2
#define LONG_PKT_FORCE_ABORT_THRESH   500
#define LONG_PACKET_QUEUE_SIZE        10

typedef enum
{
    T_LONG_PACKET_STATE_IDLE = 0,
    T_LONG_PACKET_STATE_TX   = 1,
} T_LONG_PACKET_STATE;
static volatile T_LONG_PACKET_STATE tans_state = T_LONG_PACKET_STATE_IDLE;

typedef enum
{
    T_LONG_PACKET_EVENT_START_TRANS = 0,
    T_LONG_PACKET_EVENT_ABORT_TRANS = 1,
    T_LONG_PACKET_EVENT_TRANS_DONE  = 2,
    T_LONG_PACKET_EVENT_ACK_RECV    = 3,
    T_LONG_PACKET_EVENT_NACK_RECV   = 4,
} T_LONNG_PACKET_EVENT;

typedef struct
{
    uint8_t *pkt_src;
    uint16_t len;
    T_LONG_PKT_TRANSFER_HEADER type;
    ppt_trans_handle_long_pkt_send_compl_cb pkt_cb;
} T_LONG_PACKET_QUEUE_STORE;

/*============================================================================*
 *                              Declares
 *============================================================================*/
static bool ppt_long_pkt_recv_data_reorg(T_LONG_PKT_TRANSFER_HEADER *type) RAM_FUNCTION;
static bool ppt_trans_long_pkt_state_machine(T_LONNG_PACKET_EVENT event,
                                             uint8_t *param) RAM_FUNCTION;
static void ppt_long_pkt_trans_param_reset(void) RAM_FUNCTION;
static bool ppt_trans_long_pkt_send_record(T_SEND_LONG_PKT_TABLE val) RAM_FUNCTION;
static bool ppt_trans_long_pkt_send_table_search(uint8_t seq, uint8_t *val) RAM_FUNCTION;
static bool ppt_long_pkt_request_send_data(uint8_t *pkt_src, uint16_t pkt_len,
                                           T_LONG_PKT_TRANSFER_HEADER type,
                                           ppt_trans_handle_long_pkt_send_compl_cb cb) RAM_FUNCTION;
static void ppt_trans_long_pkt_update_send_table(uint8_t seq, uint16_t offset) RAM_FUNCTION;
static uint16_t ppt_trans_long_pkt_get_offset(uint8_t seq) RAM_FUNCTION;
static uint8_t ppt_trans_long_pkt_get_header_seq(uint8_t *header) RAM_FUNCTION;
bool ppt_trans_long_pkt_ctrl_reg_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                               ppt_trans_handle_long_pkt_send_compl_cb cb) RAM_FUNCTION;
bool ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                                      ppt_trans_handle_long_pkt_send_compl_cb cb) RAM_FUNCTION;
bool ppt_trans_long_pkt_ctrl_get_data(uint8_t *next_pkt, uint8_t *len) RAM_FUNCTION;
void ppt_trans_long_pkt_parse_pkt(uint8_t *next_pkt, uint8_t len,
                                  sync_receive_info_t info) RAM_FUNCTION;
void ppt_trans_long_pkt_recv_ack(uint8_t *header, sync_send_info_t info) RAM_FUNCTION;
void ppt_trans_long_pkt_handle_send_fail(uint8_t *packet) RAM_FUNCTION;
bool ppt_trans_long_pkt_get_is_long_pkt_transmmiting(void) RAM_FUNCTION;
/*============================================================================*
 *                              Static Variables
 *============================================================================*/
T_RECV_REORG_LONG_PKT recv_reorg_data;
T_SEND_LONG_PKT_TABLE send_record_packet[SEND_PKT_TABLE_MAX_LEN];
static uint8_t send_idx = 0;
static uint8_t send_cfm_idx = 0;
static uint8_t ack_exp_seq = 0;
T_RECV_LONG_PKT_TABLE recv_record_packet[RCV_PKT_TABLE_MAX_LEN];
static uint16_t recv_idx = 0;
static bool is_recving = false;
static uint64_t transmit_ankor = UINT64_MAX;
static uint8_t long_pkt_data_buff[RCV_PKT_TABLE_MAX_LEN];
static T_LONG_PACKET_QUEUE_STORE long_pkt_trans_queue[LONG_PACKET_QUEUE_SIZE];
static uint8_t long_pkt_queue_read_idx = 0;
static uint8_t long_pkt_queue_write_idx = 0;
static uint8_t long_pkt_queue_pending_data = 0;
static T_LONG_PACKET_QUEUE_STORE link_layer_long_pkt_trans_queue[LONG_PACKET_QUEUE_SIZE];
static uint8_t link_layer_long_pkt_queue_read_idx = 0;
static uint8_t link_layer_long_pkt_queue_write_idx = 0;
static uint8_t link_layer_long_pkt_queue_pending_data = 0;

static uint8_t *long_pkt_max_recv_length = NULL;

/** pointer to long packet data */
static uint8_t *long_pkt_src = 0;
/** size of long packet data */
static uint16_t long_pkt_len = 0;
/** sequence number of current long packet */
static uint8_t long_pkt_seq_num = 0;
/** current processed long packet bytes */
static uint16_t long_pkt_offset = 0;
/** callback function when long packet send completed or flushed */
static ppt_trans_handle_long_pkt_send_compl_cb long_pkt_send_compl_cb = NULL;
/** callback function when long packet recv completed or flushed */
static ppt_trans_handle_long_pkt_recv_compl_cb long_pkt_recv_compl_cb = NULL;

#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
static const ppt_trans_handle_long_pkt_recv_compl_cb chann_ctrl_recv_compl_cb =
    ppt_tans_chann_ctrl_long_pkt_rcv_cb;
#endif


static void ppt_long_pkt_trans_param_reset(void)
{
    memset(send_record_packet, 0, sizeof(send_record_packet));
    memset(long_pkt_data_buff, 0, sizeof(long_pkt_data_buff));
    long_pkt_src = NULL;
    long_pkt_len = NULL;
    long_pkt_offset = 0;
    long_pkt_seq_num = 0;
    send_idx = 0;
    send_cfm_idx = 0;
    ack_exp_seq = 0;
    long_pkt_send_compl_cb = NULL;
}

static inline void ppt_long_pkt_recv_param_reset(void)
{
    recv_idx = 0;
    long_pkt_max_recv_length = &ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_recv;
    memset(recv_record_packet, 0, sizeof(recv_record_packet));
}

static bool ppt_long_pkt_recv_data_reorg(T_LONG_PKT_TRANSFER_HEADER *type)
{
    bool ret = true;
    memset(&recv_reorg_data, 0, sizeof(recv_reorg_data));

    for (uint16_t i = 0; i < RCV_PKT_TABLE_MAX_LEN; i++)
    {
        if (recv_record_packet[i].valid == false)
        {
            break;
        }
        memcpy(&recv_reorg_data.data[recv_reorg_data.len], recv_record_packet[i].data,
               recv_record_packet[i].len);
        recv_reorg_data.len += recv_record_packet[i].len;
    }
    if (recv_reorg_data.len < 3)
    {
        return false;
    }
    uint16_t sum = 0;
    recv_reorg_data.len = (recv_reorg_data.data[2] << 8 | recv_reorg_data.data[3]) + CRC_LEN +
                          HEADER_LEN + TOTAL_LEN;
    for (uint16_t i = 0; i < recv_reorg_data.len - 2; i++)
    {
        sum += recv_reorg_data.data[i];
    }
    if (recv_reorg_data.data[recv_reorg_data.len - 2] == (sum >> 8) &&
        recv_reorg_data.data[recv_reorg_data.len - 1] == (uint8_t)sum)
    {
        recv_reorg_data.len -= CRC_LEN;
        recv_reorg_data.len -= HEADER_LEN;
        recv_reorg_data.len -= TOTAL_LEN;
        *type = ((recv_reorg_data.data[0] << 8) | recv_reorg_data.data[1]);
    }
    else
    {
        APP_PRINT_TRACE2("[ppt_long_pkt_recv_data_reorg] unmatch %b %x",
                         TRACE_BINARY(recv_reorg_data.len, recv_reorg_data.data),
                         sum);
        ret = false;
    }

    return ret;
}

static bool ppt_trans_long_pkt_state_machine(T_LONNG_PACKET_EVENT event, uint8_t *param)
{
    bool ret = true;
    switch (event)
    {
    case T_LONG_PACKET_EVENT_START_TRANS:
        {
            if (tans_state == T_LONG_PACKET_STATE_IDLE)
            {
                tans_state = T_LONG_PACKET_STATE_TX;
                transmit_ankor = os_sys_time_get();
            }
            else
            {
                ret = false;
            }
        }
        break;
    case T_LONG_PACKET_EVENT_ABORT_TRANS:
        {
            if (long_pkt_send_compl_cb != NULL)
            {
                sync_send_info_t temp = {0};
                long_pkt_send_compl_cb(false, temp);
            }
            tans_state = T_LONG_PACKET_STATE_IDLE;
            ppt_long_pkt_trans_param_reset();

            if (link_layer_long_pkt_queue_pending_data != 0)
            {
                link_layer_long_pkt_queue_pending_data--;
                ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(
                    link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].pkt_src,
                    link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].len,
                    link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].pkt_cb);
                link_layer_long_pkt_queue_read_idx = (link_layer_long_pkt_queue_read_idx + 1) %
                                                     LONG_PACKET_QUEUE_SIZE;
            }
            else if (long_pkt_queue_pending_data != 0)
            {
                long_pkt_queue_pending_data--;
                ppt_trans_long_pkt_ctrl_reg_long_pkt_send(long_pkt_trans_queue[long_pkt_queue_read_idx].pkt_src,
                                                          long_pkt_trans_queue[long_pkt_queue_read_idx].len,
                                                          long_pkt_trans_queue[long_pkt_queue_read_idx].pkt_cb);
                long_pkt_queue_read_idx = (long_pkt_queue_read_idx + 1) % LONG_PACKET_QUEUE_SIZE;
            }
        }
        break;
    case T_LONG_PACKET_EVENT_TRANS_DONE:
        {
            tans_state = T_LONG_PACKET_STATE_IDLE;
            ppt_long_pkt_trans_param_reset();
        }
        break;
    case T_LONG_PACKET_EVENT_ACK_RECV:
        break;
    case T_LONG_PACKET_EVENT_NACK_RECV:
        break;
    default:
        ret = false;
        break;
    }
#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_TRACE2("ppt_trans_long_pkt_state_machine %d, ret %d", event, ret);
#endif
    return ret;
}

static bool ppt_trans_long_pkt_send_record(T_SEND_LONG_PKT_TABLE val)
{
    if (send_idx == ((send_cfm_idx - 1 + SEND_PKT_TABLE_MAX_LEN) % SEND_PKT_TABLE_MAX_LEN))
    {
        return false;
    }

    send_record_packet[send_idx] = val;

    send_idx = (send_idx + 1) % SEND_PKT_TABLE_MAX_LEN;

    return true;
}

static bool ppt_trans_long_pkt_send_table_search(uint8_t seq, uint8_t *val)
{
    uint8_t len = send_idx > send_cfm_idx ? send_idx - send_cfm_idx + 1 : send_idx +
                  (SEND_PKT_TABLE_MAX_LEN - send_cfm_idx);
    for (uint8_t i = 0; i < len; i ++)
    {
        uint8_t idx = (i + send_cfm_idx) % SEND_PKT_TABLE_MAX_LEN;
        if (send_record_packet[idx].valid && send_record_packet[idx].seq == seq)
        {
            *val = idx;
            return true;
        }
    }
    return false;
}

static void ppt_trans_long_pkt_update_send_table(uint8_t seq, uint16_t offset)
{
    if (long_pkt_offset < offset)
    {
        return;
    }
    long_pkt_seq_num = seq;
    long_pkt_offset = offset;
}

static uint16_t ppt_trans_long_pkt_get_offset(uint8_t seq)
{
    uint8_t i = 0;
    if (ppt_trans_long_pkt_send_table_search(seq, &i))
    {
        return send_record_packet[i].offset;
    }

    return 0xFFFF;
}

static uint8_t ppt_trans_long_pkt_get_header_seq(uint8_t *header)
{
    return ((T_LONG_PKT_HEADER *)header)->seq;
}

static void ppt_long_pkt_trans_crc_calculate(uint8_t *pkt_src, uint16_t pkt_len,
                                             T_LONG_PKT_TRANSFER_HEADER type)
{
    uint8_t *ptr = long_pkt_data_buff;
    *ptr = type >> 8;
    ptr++;
    *ptr = (uint8_t) type;
    ptr++;
    *ptr = pkt_len >> 8;
    ptr++;
    *ptr = (uint8_t) pkt_len;
    ptr++;

    uint16_t crc = (type >> 8) + (uint8_t) type + (pkt_len >> 8) + (uint8_t) pkt_len;
    for (uint8_t i = 0; i < pkt_len; i++)
    {
        *ptr = pkt_src[i];
        crc += pkt_src[i];
        ptr++;
    }
    *ptr = crc >> 8;
    ptr++;
    *ptr = (uint8_t)crc;
}

static bool ppt_long_pkt_request_send_data(uint8_t *pkt_src, uint16_t pkt_len,
                                           T_LONG_PKT_TRANSFER_HEADER type,
                                           ppt_trans_handle_long_pkt_send_compl_cb cb)
{
    uint64_t cur = os_sys_time_get();

    if (pkt_len > (RCV_PKT_TABLE_MAX_LEN - CRC_LEN - HEADER_LEN - TOTAL_LEN))
    {
        return false;
    }
    else if ((tans_state == T_LONG_PACKET_STATE_TX) && ((transmit_ankor > cur) &&
                                                        (UINT64_MAX - transmit_ankor + cur) > LONG_PKT_FORCE_ABORT_THRESH ||
                                                        (cur > transmit_ankor && cur - transmit_ankor > LONG_PKT_FORCE_ABORT_THRESH)))
    {
        ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_ABORT_TRANS, NULL);
    }

    bool ret = false;
    ret = ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_START_TRANS, NULL);

    uint32_t s = os_lock();
    if (ret)
    {
        ppt_long_pkt_trans_param_reset();
        ppt_long_pkt_trans_crc_calculate(pkt_src, pkt_len, type);
        long_pkt_src = long_pkt_data_buff;
        long_pkt_len = pkt_len + CRC_LEN + HEADER_LEN + TOTAL_LEN;
        long_pkt_offset = 0;
        long_pkt_seq_num = 0;
        long_pkt_send_compl_cb = cb;
        send_idx = 0;
        send_cfm_idx = 0;
        ack_exp_seq = 0;
    }
    else
    {
        uint8_t *pending_data, *write_idx;
        T_LONG_PACKET_QUEUE_STORE *queue;
        if (type == LINK_LAYER_PKT)
        {
            pending_data = &link_layer_long_pkt_queue_pending_data;
            write_idx = &link_layer_long_pkt_queue_write_idx;
            queue = link_layer_long_pkt_trans_queue;
        }
        else
        {
            pending_data = &long_pkt_queue_pending_data;
            write_idx = &long_pkt_queue_write_idx;
            queue = long_pkt_trans_queue;
        }

        if (*pending_data < LONG_PACKET_QUEUE_SIZE)
        {
            (*pending_data)++;
            T_LONG_PACKET_QUEUE_STORE temp;
            temp.len = pkt_len;
            temp.pkt_src = pkt_src;
            temp.pkt_cb = cb;
            temp.type = type;
            queue[*write_idx] = temp;
            *write_idx = (*write_idx + 1) % LONG_PACKET_QUEUE_SIZE;
            ret = true;
        }
    }
    os_unlock(s);
    return ret;
}
/*============================================================================*
 *                              Functions
 *============================================================================*/
/******************************************************************
 * @brief  handle register long packet sending
 * @param  pkt_src - pointer to the long packet data
 * @param  pkt_len - size of the long packet
 * @param  cb - callback function when long packet transmission completes
 * @retval void
 */
bool ppt_trans_long_pkt_ctrl_reg_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                               ppt_trans_handle_long_pkt_send_compl_cb cb)
{
    return ppt_long_pkt_request_send_data(pkt_src, pkt_len, USER_DEFINE_PKT, cb);
}

bool ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(uint8_t *pkt_src, uint16_t pkt_len,
                                                      ppt_trans_handle_long_pkt_send_compl_cb cb)
{
    bool ret = ppt_long_pkt_request_send_data(pkt_src, pkt_len, LINK_LAYER_PKT, cb);
    if (ret &&
        (false == ppt_trans_handle_get_sensor_status()))
    {
        T_PPT_TRANS_MOUSE_DATA dummy_data = {0};
        ppt_trans_pkt_ctrl_receive_data(dummy_data, true);
        ppt_trans_sync_ctrl_report_data();
    }
    return ret;
}


/******************************************************************
 * @brief  register handler for long packet receive completion
 * @param  cb - callback function when long packet receiption completes
 * @retval void
 */
void ppt_trans_long_pkt_ctrl_reg_long_pkt_recv(ppt_trans_handle_long_pkt_recv_compl_cb cb)
{
    APP_PRINT_INFO1("[ppt_trans_long_pkt_ctrl_reg_long_pkt_recv] recv cb = 0x%x", cb);
    long_pkt_recv_compl_cb = cb;
}

/******************************************************************
 * @brief  handle long packet composition
 * @param  next_pkt - pointer of current processing packet
 * @param  len - size of current processing packet, unit bytes
 * @retval void
 */
bool ppt_trans_long_pkt_ctrl_get_data(uint8_t *next_pkt, uint8_t *len)
{
    uint8_t *ptr_data = next_pkt;
    uint8_t remain_len = ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent - *len;

    uint64_t cur = os_sys_time_get();

    if ((tans_state == T_LONG_PACKET_STATE_TX) && ((transmit_ankor > cur) &&
                                                   (UINT64_MAX - transmit_ankor + cur) > LONG_PKT_FORCE_ABORT_THRESH ||
                                                   (cur > transmit_ankor && cur - transmit_ankor > LONG_PKT_FORCE_ABORT_THRESH)))
    {
        ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_ABORT_TRANS, NULL);
        return false;
    }

    if (remain_len <= 1 || long_pkt_len <= long_pkt_offset)
    {
        return false;
    }

#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_INFO4("[ppt_trans_long_pkt_ctrl_get_data] org pkt len: %d data: [%b], %d %d",
                    *len, TRACE_BINARY(*len, next_pkt), send_idx, send_cfm_idx);
#endif
    T_LONG_PKT_HEADER long_pkt;
    T_PPT_TRANS_PKT_HEADER *org_header;
    uint32_t s = os_lock();

    org_header = (T_PPT_TRANS_PKT_HEADER *)&ptr_data[0];
    org_header -> long_pkt_en = 1;
    ptr_data += (*len);
    //long pkt header
    (*len)++;
    remain_len--;

    if (long_pkt_offset == 0)
    {
        if (long_pkt_len - long_pkt_offset <= remain_len)
        {
            remain_len = long_pkt_len - long_pkt_offset;
            long_pkt.packet_type = LONG_PKT_TYPE_FIRST_AND_END;
        }
        else
        {
            long_pkt.packet_type = LONG_PKT_TYPE_FIRST_PKT;
        }
    }
    else if (long_pkt_len - long_pkt_offset <= remain_len)
    {
        remain_len = long_pkt_len - long_pkt_offset;
        long_pkt.packet_type = LONG_PKT_TYPE_END_PKT;
    }
    else
    {
        long_pkt.packet_type = LONG_PKT_TYPE_CONT_PKT;
    }

    long_pkt.seq = long_pkt_seq_num;
    *ptr_data = long_pkt.d8;
    ptr_data++;

    ppt_trans_long_pkt_send_table_search(long_pkt.seq, &send_idx);
    T_SEND_LONG_PKT_TABLE temp;

    temp.valid = true;
    temp.seq = long_pkt.seq;
    temp.offset = long_pkt_offset;
    ppt_trans_long_pkt_send_record(temp);

    while (remain_len > 0)
    {
        *ptr_data = *(long_pkt_src + long_pkt_offset);
        ptr_data++;
        (*len)++;
        remain_len--;
        long_pkt_offset++;
    }
#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_INFO5("[ppt_trans_long_pkt_ctrl_get_data] long_pkt_seq_num: %d, send_idx + 1: [%d], %b, remain_len: %d %d"
                    ,
                    long_pkt_seq_num, send_idx, TRACE_BINARY(*len, next_pkt), long_pkt_len, long_pkt_offset);
#endif
    long_pkt.seq++;
    long_pkt_seq_num = long_pkt.seq;
    os_unlock(s);
    return true;
}

/******************************************************************
 * @brief  handle long packet parsing
 * @param  next_pkt - pointer of current processing packet
 * @param  len - size of long packet, unit bytes
 * @param  info - receive info recorded by sync layer
 * @retval void
 */
void ppt_trans_long_pkt_parse_pkt(uint8_t *next_pkt, uint8_t len, sync_receive_info_t info)
{
#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_long_pkt_parse_pkt] len: %d data: [%b]",
                    len, TRACE_BINARY(len, next_pkt));
#endif

    T_LONG_PKT_HEADER long_pkt;
    uint32_t s = os_lock();

    len--;
    long_pkt.d8 = *next_pkt;

    if (long_pkt.packet_type == LONG_PKT_TYPE_FIRST_PKT ||
        long_pkt.packet_type == LONG_PKT_TYPE_FIRST_AND_END)
    {
        recv_idx = 0;
        is_recving = true;
    }
    if (!is_recving)
    {
        goto func_exit;
    }

    uint16_t write_idx = recv_idx;
    uint8_t back_find = recv_idx >= RUNNING_WINDOW_MAX_LEN ? recv_idx - RUNNING_WINDOW_MAX_LEN : 0;
    {
        for (uint8_t i = recv_idx; i > back_find; i--)
        {
            if (recv_record_packet[i].valid && recv_record_packet[i].seq == long_pkt.seq)
            {
                write_idx = i;
                break;
            }
        }
    }

    memset(&recv_record_packet[write_idx], 0,
           sizeof(T_RECV_LONG_PKT_TABLE) * (RCV_PKT_TABLE_MAX_LEN - write_idx));

    if (write_idx != 0)
    {
        if (recv_record_packet[write_idx - 1].valid == false)
        {
            APP_PRINT_ERROR2("[ppt_trans_long_pkt_parse_pkt] missing seq, valid false %d, %d", write_idx,
                             long_pkt.seq);
            goto func_exit;
        }
        T_LONG_PKT_HEADER prev_pkt;
        prev_pkt.seq = recv_record_packet[write_idx - 1].seq + 1;
        if (prev_pkt.seq != long_pkt.seq)
        {
            APP_PRINT_ERROR3("[ppt_trans_long_pkt_parse_pkt] missing seq %d, %d, expected: %d", write_idx,
                             long_pkt.seq, prev_pkt.seq);
            goto func_exit;
        }
    }

    next_pkt++;
    recv_record_packet[write_idx].valid = true;
    recv_record_packet[write_idx].seq = long_pkt.seq;
    recv_record_packet[write_idx].len = MIN(len, *long_pkt_max_recv_length - 1);
    memcpy(recv_record_packet[write_idx].data, next_pkt, len);
    if (long_pkt.packet_type == LONG_PKT_TYPE_END_PKT ||
        long_pkt.packet_type == LONG_PKT_TYPE_FIRST_AND_END)
    {
        T_LONG_PKT_TRANSFER_HEADER type = INVALID_HEADER;
        if (ppt_long_pkt_recv_data_reorg(&type))
        {
            if (type == USER_DEFINE_PKT && long_pkt_recv_compl_cb != NULL)
            {
                long_pkt_recv_compl_cb(true, recv_reorg_data.data + HEADER_LEN + TOTAL_LEN, recv_reorg_data.len,
                                       info);
            }
            else
            {
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
                chann_ctrl_recv_compl_cb(true, recv_reorg_data.data + HEADER_LEN + TOTAL_LEN, recv_reorg_data.len,
                                         info);
#endif
            }
            is_recving = false;
        }
    }
    else
    {
        recv_idx = write_idx + 1;
    }
func_exit:
    os_unlock(s);
}


/******************************************************************
 * @brief  init PPT transport layer long packet control & managements
 * @retval void
 */
void ppt_trans_long_pkt_ctrl_init(void)
{
    ppt_long_pkt_trans_param_reset();
    ppt_long_pkt_recv_param_reset();
}

void ppt_trans_long_pkt_recv_ack(uint8_t *header, sync_send_info_t info)
{
    T_LONG_PKT_HEADER recv_header = *((T_LONG_PKT_HEADER *) header);
#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_TRACE3("[ppt_trans_long_pkt_recv_ack] %x %d, %d", recv_header, recv_header.seq,
                     ack_exp_seq);
#endif

    uint32_t s = os_lock();
    if (recv_header.seq != ack_exp_seq)
    {
//        uint16_t offset = ppt_trans_long_pkt_get_offset(ack_exp_seq);
//        ppt_trans_long_pkt_update_send_table(ack_exp_seq, offset);
        goto func_exit;
    }
    T_LONG_PKT_HEADER temp = recv_header;
    temp.seq ++;
    ack_exp_seq = temp.seq;

    if (ppt_trans_long_pkt_send_table_search(recv_header.seq, &send_cfm_idx))
    {
        send_cfm_idx = (send_cfm_idx + SEND_PKT_TABLE_MAX_LEN - RUNNING_WINDOW_MAX_LEN) %
                       SEND_PKT_TABLE_MAX_LEN;
    }

    if (recv_header.packet_type == LONG_PKT_TYPE_END_PKT ||
        recv_header.packet_type == LONG_PKT_TYPE_FIRST_AND_END)
    {
        if (long_pkt_send_compl_cb != NULL)
        {
            ppt_trans_handle_long_pkt_send_compl_cb temp_cb = long_pkt_send_compl_cb;
            ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_TRANS_DONE, NULL);
            temp_cb(true, info);
        }
        else
        {
            ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_TRANS_DONE, NULL);
        }

        if (link_layer_long_pkt_queue_pending_data != 0)
        {
            link_layer_long_pkt_queue_pending_data--;
            ppt_trans_long_pkt_ctrl_link_layer_long_pkt_send(
                link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].pkt_src,
                link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].len,
                link_layer_long_pkt_trans_queue[link_layer_long_pkt_queue_read_idx].pkt_cb);
            link_layer_long_pkt_queue_read_idx = (link_layer_long_pkt_queue_read_idx + 1) %
                                                 LONG_PACKET_QUEUE_SIZE;
        }
        else if (long_pkt_queue_pending_data != 0)
        {
            long_pkt_queue_pending_data--;
            ppt_trans_long_pkt_ctrl_reg_long_pkt_send(long_pkt_trans_queue[long_pkt_queue_read_idx].pkt_src,
                                                      long_pkt_trans_queue[long_pkt_queue_read_idx].len,
                                                      long_pkt_trans_queue[long_pkt_queue_read_idx].pkt_cb);
            long_pkt_queue_read_idx = (long_pkt_queue_read_idx + 1) % LONG_PACKET_QUEUE_SIZE;
        }
    }
func_exit:
    os_unlock(s);
}

/******************************************************************
 * @brief  handle long packet send fail
 * @param  packet - pointer of the failed packet
 * @retval void
 */
void ppt_trans_long_pkt_handle_send_fail(uint8_t *packet)
{
    uint32_t s = os_lock();
    uint8_t seq = ppt_trans_long_pkt_get_header_seq(packet);

    uint16_t offset = ppt_trans_long_pkt_get_offset(seq);
    ppt_trans_long_pkt_update_send_table(seq, offset);
    uint16_t ack_offset = ppt_trans_long_pkt_get_offset(ack_exp_seq);
    if (offset < ack_offset)
    {
        ack_exp_seq = seq;
    }

    T_LONG_PKT_HEADER header;
    header.seq = seq + 1;
    uint8_t i = 0;
    while (ppt_trans_long_pkt_send_table_search(header.seq, &i))
    {
        if (i == send_cfm_idx)
        {
            break;
        }
        send_record_packet[i].valid = false;
        header.seq++;
    }

    os_unlock(s);
#if PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN
    APP_PRINT_TRACE2("ppt_trans_long_pkt_handle_send_fail, fail seq %d, %b", seq, TRACE_BINARY(5,
                     packet));
#endif
}

/******************************************************************
 * @brief  To get whether the device is transmitting long packet or not.
 * @retval bool
 */
bool ppt_trans_long_pkt_get_is_long_pkt_transmmiting(void)
{
    return tans_state == T_LONG_PACKET_STATE_TX;
}

/******************************************************************
 * @brief   handle long packet module when 2.4G master and slave disconnected.
 * @return  none
 * @retval  void
 */
void ppt_trans_long_pkt_handle_connect(void)
{
    long_pkt_queue_read_idx = 0;
    long_pkt_queue_write_idx = 0;
    long_pkt_queue_pending_data = 0;
    link_layer_long_pkt_queue_read_idx = 0;
    link_layer_long_pkt_queue_write_idx = 0;
    link_layer_long_pkt_queue_pending_data = 0;
    ppt_trans_long_pkt_state_machine(T_LONG_PACKET_EVENT_ABORT_TRANS, NULL);
}

#endif //FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K