/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>
#include "trace.h"
#include "button_handle.h"
#include "ble/hid.h"
#include "ble/hog.h"
#include "ble/ble.h"
#include "voice/voice_driver.h"
#include "ppt/voice_ppt_master.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/*============================================================================*
 *                     Workqueue deferred items
 *
 * GPIO callbacks run in ISR context.  voice_handle_stop_mic() calls
 * k_sem_take(K_MSEC(500)) to wait for the rx thread to finish — that
 * blocks, which is illegal in ISR context and returns -EWOULDBLOCK
 * immediately.  Defer press/release to the system workqueue so they
 * run in thread context where blocking is allowed.
 *============================================================================*/
static void press_work_fn(struct k_work *w)
{
    ARG_UNUSED(w);
    LOG_DBG("[key_handle] press work: voice mic pressed");
    voice_handle_mic_key_pressed();
}

static void release_work_fn(struct k_work *w)
{
    ARG_UNUSED(w);
    LOG_DBG("[key_handle] release work: voice mic released");
    voice_handle_mic_key_released();
}

static K_WORK_DEFINE(press_work,   press_work_fn);
static K_WORK_DEFINE(release_work, release_work_fn);

/******************************************************************
 * @brief    handle key release event (ISR-safe via workqueue)
 * @param    none
 * @return   none
 * @retval   void
 */
void key_handle_release_event(void)
{
    LOG_DBG("[key_handle_release_event] key release event");

    if (voice_driver_global_data.is_voice_driver_working == true) {
        k_work_submit(&release_work);
    }
}

/******************************************************************
 * @brief  handle one key pressed scenario (ISR-safe via workqueue)
 * @param  none
 * @return none
 * @retval void
 */
void key_handle_one_key_scenario(void)
{
#if IS_ENABLED(CONFIG_BT)
    T_BLE_STATUS ble_status = get_ble_status();
    switch (ble_status) {
    case BLE_STATUS_IDLE:
    case BLE_STATUS_ADVERTISING:
    case BLE_STATUS_PAIRED:
        LOG_DBG("[key_handle_one_key_scenario] voice mic press");
        k_work_submit(&press_work);
        break;
    default:
        break;
    }
#else
    voice_ppt_master_try_reconnect();
    k_work_submit(&press_work);
#endif
}