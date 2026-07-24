/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_test_handle.c
   * @brief     transport layer test module scenario handler
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
#include "trace.h"
#include "string.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_test_handle.h"
#include "ppt_trans_test_stub_sync.h"

#if PPT_TRANS_TEST_ENABLE
/*============================================================================*
 *                              Variables
 *============================================================================*/
uint8_t success_mask[8] = {1, 1, 1, 1, 1, 1, 1, 1};
uint32_t report_interval = PPT_TRANS_TEST_SYNC_REPORT_ITVL;
T_PPT_TRANS_MOUSE_DATA test_data[] =
{
    {
        .button = 0x7,
        .optical_x = 0x7f,
        .optical_y = 0x0,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
    {
        .button = 0,
        .optical_x = 0x0,
        .optical_y = 0x7f,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
    {
        .button = 0x0,
        .optical_x = 0x0,
        .optical_y = 0x81,
        .wheel_direction = PPT_TRANS_WHEEL_H_UP_DEF,
    },
    {
        .button = 0x7,
        .optical_x = 0x81,
        .optical_y = 0x0,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
    {
        .button = 0x7,
        .optical_x = 0x7,
        .optical_y = 0x0,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
    {
        .button = 0,
        .optical_x = 0x0,
        .optical_y = 0x7,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
    {
        .button = 0x0,
        .optical_x = 0x0,
        .optical_y = 0xf9,
        .wheel_direction = PPT_TRANS_WHEEL_H_DOWN_DEF,
    },
    {
        .button = 0x7,
        .optical_x = 0xf9,
        .optical_y = 0x0,
        .wheel_direction = PPT_TRANS_WHEEL_RELEASE_DEF,
    },
};

/*============================================================================*
 *                              Static functions
 *============================================================================*/
/**
 * @brief Report callback for stub_sync submodule to report current result
 *
 * @param test_cnt - how many test cases have been adopted currently
 * @param result   - current result
 */
static void ppt_trans_test_handle_sync_test_cb(uint32_t test_cnt,
                                               T_PPT_TRANS_TEST_STUB_SYNC_REPORT_FMT result)
{
    APP_PRINT_INFO5("[ppt_trans_test_handle_sync_test_cb] Trial %d, result: (%d, %d), wheel: (%d, %d) (H,V)",
                    test_cnt, result.motion_x, result.motion_y, result.wheel_h_dir, result.wheel_v_dir);
    APP_PRINT_INFO2("[ppt_trans_test_handle_sync_test_cb] button press cnt: [%b], button release cnt: [%b]",
                    TRACE_BINARY(PPT_PKT_PAYLOAD_SIZE_BUTTON, result.button_press),
                    TRACE_BINARY(PPT_PKT_PAYLOAD_SIZE_BUTTON, result.button_press));
}

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief send pre-defined testcase out to replace real optical sensor data
*/
void ppt_trans_test_handle_get_testcase(T_PPT_TRANS_MOUSE_DATA *data)
{
    static uint8_t test_index = 0;
    static uint8_t test_size = sizeof(test_data) / sizeof(T_PPT_TRANS_MOUSE_DATA);
    memcpy(data, test_data + test_index, sizeof(T_PPT_TRANS_MOUSE_DATA));
    test_index = (test_index + 1) % test_size;
}

/**
 * @brief init test module
*/
void ppt_trans_test_handle_init(void)
{
    ppt_trans_test_stub_sync_init();
    ppt_trans_test_stub_sync_set_test_param(PPT_TRANS_TEST_STUB_SYNC_PARAM_SUCCESS_MASK,
                                            (void *) success_mask);
    ppt_trans_test_stub_sync_set_test_param(PPT_TRANS_TEST_STUB_SYNC_PARAM_REPORT_CNT,
                                            (void *) &report_interval);
    ppt_trans_test_stub_sync_set_test_param(PPT_TRANS_TEST_STUB_SYNC_PARAM_REG_REPORT_CB,
                                            (void *) ppt_trans_test_handle_sync_test_cb);
}

#endif // PPT_TRANS_TEST_ENABLE
