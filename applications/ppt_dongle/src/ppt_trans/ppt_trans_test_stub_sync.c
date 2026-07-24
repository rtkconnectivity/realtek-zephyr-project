/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_test_stub_sync.c
   * @brief     transport layer test module for sync lib testing
   * @author    luke
   * @date      2023-10-06
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */


/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include "trace.h"
#include "ppt_sync.h"
#include "app_section.h"
#include "ppt_trans_pkt_algo.h"
#include "ppt_trans_test_handle.h"
#include "ppt_trans_test_stub_sync.h"

#if PPT_TRANS_TEST_ENABLE
/*============================================================================*
 *                              Static variables
 *============================================================================*/
/** mask indicating packets should be successful,
 *  success_mask[i] = 1 -> ith packet should success for every 8 packets
 *  note that 8th packet must fail according to sync lib's 7T1R mechanism.
 */
static uint8_t success_mask[8] = {0};
/** packet sent counter in each round to contrl success rate */
static uint8_t packet_cnt_total = 0;
/** message send callback function of sync_ctrl submodule */
static sync_msg_send_cb_t sync_ctrl_cb_fp = NULL;
/** how many testcases have been tested */
static uint32_t packet_test_cnt = 0;
/** when should report test case */
static uint32_t packet_report_cnt = 0;
/** report test result callback*/
mouse_ppt_test_stub_sync_report_cb report_fp = NULL;
/** current received sequence number */
static uint8_t cur_seq_num = 0xFF;
/** current received sequence number */
T_PPT_TRANS_TEST_STUB_SYNC_REPORT_FMT result = {0};

/*============================================================================*
 *                              Function Declarations
 *============================================================================*/
void ppt_trans_test_stub_sync_send(sync_msg_type_t type, uint8_t *data, uint16_t len,
                                   sync_msg_send_cb_t send_cb) RAM_FUNCTION;

/*============================================================================*
 *                              Static functions
 *============================================================================*/


/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief stub-test submodule handler for send request from DUT module
 *
 * @param type - msg type, different types have different retransmit behavior
 * @param data - the data buffer
 * @param len - the data length
 * @param send_cb - the send complete callback
 */
void ppt_trans_test_stub_sync_send(sync_msg_type_t type, uint8_t *data, uint16_t len,
                                   sync_msg_send_cb_t send_cb)
{
    uint32_t s = os_lock();
    sync_ctrl_cb_fp = send_cb;
    os_unlock(s);
#if PPT_TRANS_TEST_STUB_SYNC_DBG_LOG_EN
    APP_PRINT_INFO2("[ppt_trans_test_stub_sync_send] len: %d, data: [%b]",
                    len, TRACE_BINARY(len, data));
#endif
    sync_send_info_t res;
    res.retrans_count = 0;
    if (success_mask[packet_cnt_total] == 0)
    {
        res.res = SYNC_SEND_RESULT_UNACKED;
    }
    else
    {
        res.res = SYNC_SEND_RESULT_ACKED;
        T_PPT_TRANS_MOUSE_DATA data_out[PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV] = {0};
        T_PPT_TRANS_PKT_HEADER header;
        header.d8 = data[0];
        uint8_t length;
        uint8_t seq_num;
        uint8_t used_bytes = 0;
        ppt_trans_pkt_algo_parse_pkt(data, len, data_out, &length, &seq_num, &used_bytes);
        if (header.long_pkt_en)
        {
            ppt_trans_long_pkt_parse_pkt(data + used_bytes, len - used_bytes);
        }
        uint8_t points = ((seq_num + PPT_PKT_PAYLOAD_SEQ_NUM_SIZE - cur_seq_num) %
                          PPT_PKT_PAYLOAD_SEQ_NUM_SIZE);
        points = (points > PPT_PKT_PAYLOAD_DATA_NUM_MAX) ? PPT_PKT_PAYLOAD_DATA_NUM_MAX : points;
        for (uint8_t i = 0; i < points; i++)
        {
            result.motion_x += data_out[length - 1 - i].optical_x;
            result.motion_y += data_out[length - 1 - i].optical_y;
#if PPT_TRANS_TEST_STUB_SYNC_DBG_LOG_EN
            APP_PRINT_INFO5("[ppt_trans_test_stub_sync_send] seq %d adpoted, x: 0x%x, y: 0x%x, accumulated x: %d, y: %d",
                            seq_num - i, data_out[length - 1 - i].optical_x, data_out[length - 1 - i].optical_y,
                            result.motion_x, result.motion_y);
#endif
        }

        uint8_t button = data_out[length - 1].button;
        for (uint8_t i = 0; i < PPT_PKT_PAYLOAD_SIZE_BUTTON; i++)
        {
            if (button & 0x01)
            {
                result.button_press[i] += 1;
            }
            else
            {
                result.button_release[i] += 1;
            }
            button >>= 1;
        }
        T_PPT_TRANS_WHEEL_DIRECTION wheel = data_out[length - 1].wheel_direction;
        switch (wheel)
        {
        case PPT_TRANS_WHEEL_V_UP_DEF:
            {
                result.wheel_v_dir += 1;
                break;
            }
        case PPT_TRANS_WHEEL_V_DOWN_DEF:
            {
                result.wheel_v_dir -= 1;
                break;
            }
        case PPT_TRANS_WHEEL_H_UP_DEF:
            {
                result.wheel_h_dir += 1;
                break;
            }
        case PPT_TRANS_WHEEL_H_DOWN_DEF:
            {
                result.wheel_h_dir -= 1;
                break;
            }

        default:
            break;
        }
#if PPT_TRANS_TEST_STUB_SYNC_DBG_LOG_EN
        APP_PRINT_INFO3("[ppt_trans_test_stub_sync_send] wheel dir: %d, accumulated wheel_v: %d, accumulated wheel_h: %d",
                        wheel, result.wheel_v_dir, result.wheel_h_dir);
#endif
        cur_seq_num = seq_num;
    }
    packet_cnt_total = (packet_cnt_total + 1) % 8;
    packet_test_cnt++;

    if ((report_fp != NULL) &&
        (packet_report_cnt != 0) &&
        (packet_test_cnt % packet_report_cnt == 0))
    {
        report_fp(packet_test_cnt, result);
    }
    sync_ctrl_cb_fp(type, data, len, res);
}

/**
 * @brief stub-test submodule handler for testing parameter configuration from test handle module
 *
 * @param param_type - which parameter to configure @ref PPT_TRANS_TEST_STUB_SYNC_PARAMS
 * @param param_val  - specified parameter value
 */
void ppt_trans_test_stub_sync_set_test_param(PPT_TRANS_TEST_STUB_SYNC_PARAMS param_type,
                                             void *param_val)
{
    APP_PRINT_INFO1("[ppt_trans_test_stub_sync_set_test_param] param type: %d", param_type);

    switch (param_type)
    {
    case PPT_TRANS_TEST_STUB_SYNC_PARAM_SUCCESS_MASK:
        {
            memcpy(success_mask, (uint8_t *) param_val, sizeof(success_mask));
            break;
        }

    case PPT_TRANS_TEST_STUB_SYNC_PARAM_REPORT_CNT:
        {
            packet_report_cnt = *((uint32_t *)(param_val));
            APP_PRINT_INFO1("[ppt_trans_test_stub_sync_set_test_param] report cnt = %d", packet_report_cnt);
            break;
        }

    case PPT_TRANS_TEST_STUB_SYNC_PARAM_REG_REPORT_CB:
        {
            report_fp = (mouse_ppt_test_stub_sync_report_cb)param_val;
            break;
        }

    default:
        {
            APP_PRINT_INFO0("[ppt_trans_test_stub_sync_set_test_param] unsupported test param");
            break;
        }
    }
}

/**
 * @brief stub-test submodule initializer
 *
 */
void ppt_trans_test_stub_sync_init(void)
{
    memset(success_mask, 0, sizeof(success_mask));
    packet_cnt_total = 0;
    packet_test_cnt = 0;
    packet_report_cnt = 0;
    report_fp = NULL;
    memset(&result, 0, sizeof(result));
    srand(5);
}

#endif // PPT_TRANS_TEST_ENABLE
