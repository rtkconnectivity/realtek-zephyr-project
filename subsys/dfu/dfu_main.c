/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DFU, CONFIG_DFU_LOG_LEVEL);

#include "gap.h"
#include "profile_server.h"
#include "os_timer.h"
#include "otp_config.h" //todo: when oepn otp needn't
#include "otp.h"
#include "dfu_api.h"
#include "dfu_flash.h"
#include "dfu_service.h"
#include "dfu_main.h"
#include "dfu_task.h"
#include "dfu_application.h"
#include "board.h"

#if (SUPPORT_NORMAL_OTA == 1)
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define TIMER_ID_DFU_TOTAL          1
#define TIMER_ID_DFU_WAIT4_CONN     2
#define TIMER_ID_DFU_IMAGE_TRANSFER 3
#define TIMER_ID_DFU_CTITTV         4

#define BD_ADDR_SIZE 6

/* What is the advertising interval when device is discoverable (units of 625us, 160=100ms)*/
#define DEFAULT_ADVERTISING_INTERVAL_MIN 160 /* 100ms */
#define DEFAULT_ADVERTISING_INTERVAL_MAX 176 /* 110ms */

/*============================================================================*
 *                               Types
 *============================================================================*/

/*============================================================================*
 *                               Variables
 *============================================================================*/
void *total_timer_handle;
void *wait4_conn_timer_handle;
void *image_transfer_timer_handle;
void *ctittv_timer_handle;
uint32_t timeout_value_total;
uint32_t timeout_value_wait4_conn;
uint32_t timeout_value_image_transfer;
uint32_t timeout_value_ctittv;
T_SERVER_ID rtk_dfu_service_id;

void dfu_monitor_timeout_handler(void *p_xtimer)
{
    uint32_t timer_id = 0;

    os_timer_id_get(&p_xtimer, &timer_id);

    LOG_ERR("dfu_monitor_timeout_handler, TimerID(%u)", timer_id);

    switch (timer_id)
    {
    case TIMER_ID_DFU_TOTAL:
    case TIMER_ID_DFU_WAIT4_CONN:
    case TIMER_ID_DFU_IMAGE_TRANSFER:
    case TIMER_ID_DFU_CTITTV:
        dfu_fw_reboot(false);
        break;
    }
}

void dfu_timer_init(void)
{

    timeout_value_total = OTP->ota_timeout_total * 1000;
    timeout_value_wait4_conn = OTP->ota_timeout_wait4_conn * 1000;
    timeout_value_image_transfer = OTP->ota_timeout_wait4_image_transfer * 1000;
    timeout_value_ctittv =
        OTP->ota_timeout_ctittv * 1000; // will open when rom otp table is fixed

    os_timer_create(&total_timer_handle, "dfuTotalTimer", TIMER_ID_DFU_TOTAL,
                    timeout_value_total, false, dfu_monitor_timeout_handler);

    os_timer_create(&wait4_conn_timer_handle, "dfuWait4ConTimer", TIMER_ID_DFU_WAIT4_CONN,
                    timeout_value_wait4_conn, false, dfu_monitor_timeout_handler);

    os_timer_create(&image_transfer_timer_handle, "dfuImageTransferTimer",
                    TIMER_ID_DFU_IMAGE_TRANSFER, timeout_value_image_transfer, false,
                    dfu_monitor_timeout_handler);

    os_timer_create(&ctittv_timer_handle, "dfuCtittvTimer", TIMER_ID_DFU_CTITTV,
                    timeout_value_ctittv, false, dfu_monitor_timeout_handler);

    os_timer_start(&total_timer_handle);

    os_timer_start(&wait4_conn_timer_handle);
}
#endif /* SUPPORT_NORMAL_OTA */
void dfu_main(void)
{
    LOG_INF("Enter DFU mode");
    dfu_add_service(NULL);
    /* le_gap_init(1);
     * gap_lib_init();
     * dfu_le_gap_init();
     * dfu_le_profile_init();
     * dfu_init();
     */
}
