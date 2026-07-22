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

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

/*============================================================================*
 *                          Local Variables
 *============================================================================*/
//T_KEY_HANDLE_GLOBAL_DATA key_handle_global_data;  /* Value to indicate the reconnection key data */

/******************************************************************
 * @brief    handle key release event
 * @param    none
 * @return   none
 * @retval   void
 */
void key_handle_release_event(void)
{
    LOG_DBG("[key_handle_release_event] key release event");

    if (voice_driver_global_data.is_voice_driver_working == true)
    {
        voice_handle_mic_key_released();
    }

    // T_BLE_STATUS ble_status = get_ble_status();
    // if (ble_status == BLE_STATUS_PAIRED)
    // {
    //     key_handle_notify_hid_release_event();
    // }
}

/******************************************************************
 * @brief  handle one key pressed scenario
 * @param  key index - pressed key index
 * @return none
 * @retval void
 */
void key_handle_one_key_scenario(void)
{
#if IS_ENABLED(CONFIG_BT)
    T_BLE_STATUS ble_status = get_ble_status();
    switch (ble_status)
    {
    case BLE_STATUS_IDLE:
    case BLE_STATUS_ADVERTISING:
    case BLE_STATUS_PAIRED:
        {
            LOG_DBG("voice_handle_mic_key_pressed");
            voice_handle_mic_key_pressed();
        }
        break;
    default:
        break;
    }
#else
    voice_handle_mic_key_pressed();
#endif
}