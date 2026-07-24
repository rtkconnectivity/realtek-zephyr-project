/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "board.h"
#include "ppt_sync.h"
#include "ppt_sync_app.h"
#include <zephyr/kernel.h>
#include "zephyr/logging/log.h"

#if 1
#define APP_PRINT_INFO0         LOG_INF
#define APP_PRINT_INFO1         LOG_INF
#define APP_PRINT_INFO2         LOG_INF
#define APP_PRINT_INFO3         LOG_INF
#define APP_PRINT_INFO4         LOG_INF
#define APP_PRINT_INFO5         LOG_INF
#define APP_PRINT_INFO6         LOG_INF
#define APP_PRINT_INFO7         LOG_INF
#define APP_PRINT_INFO8         LOG_INF
#define APP_PRINT_WARN0         LOG_INF
#define APP_PRINT_WARN1         LOG_INF
#define APP_PRINT_WARN2         LOG_INF
#define APP_PRINT_WARN3         LOG_INF
#define APP_PRINT_WARN4         LOG_INF
#define APP_PRINT_WARN5         LOG_INF
#define APP_PRINT_WARN6         LOG_INF
#define APP_PRINT_WARN7         LOG_INF
#define APP_PRINT_WARN8         LOG_INF
#define APP_PRINT_ERROR0        LOG_INF
#define APP_PRINT_ERROR1        LOG_INF
#define APP_PRINT_ERROR2        LOG_INF
#define APP_PRINT_ERROR3        LOG_INF
#define APP_PRINT_ERROR4        LOG_INF
#define APP_PRINT_ERROR5        LOG_INF
#define APP_PRINT_ERROR6        LOG_INF
#define APP_PRINT_ERROR7        LOG_INF
#define APP_PRINT_ERROR8        LOG_INF
#define DBG_DIRECT              LOG_INF
#define TRACE_BINARY(len, pointer)  (len)
#endif

#define SEND_RELEASE_DATA_TIMEOUT       1000 /* 1s */
#if (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
#define REPAIR_OR_RECONNECT_PERIOD      100 /* 100ms */
#define ALLOW_TO_REPAIR_TIME            300 /* 300*REPAIR_OR_RECONNECT_PERIOD = 30s */
#endif
#define PPT_PAIR_RSSI_THRESHOLD         -65

#define PPT_DEFAULT_REPORT_RATE         4000
#define PPT_DEFAULT_INTERVAL_TIME       250 /* 250us */

#define DPI_DATA_SIZE                       1
#define REPORT_REATE_DATA_SIZE              2

#if !FEATURE_SUPPORT_PROPRIETARY_HOPPING
#define PPT_TRANS_SYNC_CHANS                2425, 2432, 2447, 2450, 2462, 2477, 2407, 2422, 2437, 2449, 2452, 2479, 2442, 2457, 2472, 2413, 2424, 2478
#endif

#if FEATURE_SUPPORT_PPT_DFU
#define PPT_DFU_GET_REMOTE_VER_CMD       0x47, 0x45, 0x54, 0x56, 0x45, 0x52 //GETVER
#endif

#define PPT_FAST_PAIR_STATUS_NOTIFY_TIME 100 //100ms

#define HID_KEYBOARD_REPORT_SIZE 18
#define HID_CONSUMER_REPORT_SIZE 6

typedef enum
{
    DONGLE_PPT_STATUS_DEFAULT = 0,      /* default status */
    DONGLE_PPT_STATUS_IDLE,             /* idle status */
    DONGLE_PPT_STATUS_PAIRING,          /* start sync but not paired */
    DONGLE_PPT_STATUS_PAIRED,           /* paired success status */
    DONGLE_PPT_STATUS_CONNECTING,       /* start sync but not connected */
    DONGLE_PPT_STATUS_CONNECTED,        /* connected success status */
    DONGLE_PPT_STATUS_LOW_POWER,        /* low power mode*/
} T_DONGLE_PPT_STATUS;

typedef enum
{
    PPT_DISCONN_REASON_IDLE = 0,
    PPT_DISCONN_REASON_PAIRING,
    PPT_DISCONN_REASON_TIMEOUT,
    PPT_DISCONN_REASON_PAIR_FAILED,
    PPT_DISCONN_REASON_LOW_POWER,
    PPT_DISCONN_REASON_UART_CMD,
} T_PPT_DISCONN_REASON;

typedef struct t_app_ppt_global_data
{
    bool is_mp_test_mode_by_usb;
    bool is_pptrf_test_mode;
#if FAST_PAIR_TEST_MODE
    bool is_ble_fast_pair_mode;
    bool is_ppt_fast_pair_mode;
#endif
#if FEATURE_SUPPORT_RF_TEST_MODE_BY_UART
    bool is_rf_test_mode_by_uart;
#endif
    bool is_ppt_bond; /* to indicate whether dongle is bond */
    T_DONGLE_PPT_STATUS dongle_ppt_status;
    uint8_t mouse_data_current_seq_num;
    bool is_aon_wdg_enable;
    bool is_enable_to_receive_data;
    uint32_t report_rate;
} T_APP_PPT_GLOBAL_DATA;
extern T_APP_PPT_GLOBAL_DATA ppt_app_global_data;

#if FEATURE_SUPPORT_PPT_DFU
extern uint8_t ppt_dfu_data_seq_num;
#endif

#if (DONGLE_REPAIR_MODE == ALLOW_TO_REPAIR_WITHIN_LIMITED_TIME_AFTER_RESET)
extern struct k_timer repair_after_reset_timer;
#endif

void pm_check_status_before_enter_wfi_or_dlps(void);
void pm_no_check_status_before_enter_wfi(void);
void app_system_reset(uint8_t reset_reason);
void app_init_ppt_global_data(void);
void dongle_ppt_reconnect(void);
void dongle_ppt_init(void);
void dongle_ppt_enable(void);
void dongle_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count, uint8_t *data,
                          uint16_t len);
void dongle_app_send_get_bat_value(void);
void dongle_ppt_stop_sync(void);
void dongle_app_send_led_status(uint8_t led_status);
#if CLEAR_BOND_INFO_WHEN_FACTORY_TEST
void dongle_app_send_ppt_fast_pair_status(void);
#endif
#if FEATURE_SUPPORT_PPT_DFU
void dongle_app_send_ppt_dfu(uint8_t *ppt_dfu_data, uint8_t dfu_data_len, bool need_ppt_dfu_seq);
#endif
bool dongle_ppt_trans_rx_raw_cb(uint8_t *p_data, uint16_t len, sync_receive_info_t *info);
