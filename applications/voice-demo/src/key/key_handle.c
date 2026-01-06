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
#include "key_handle.h"
#include "ble/hid.h"
#include "ble/hog.h"
#include "ble/ble.h"
#include "voice/voice_driver.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

// static const T_KEY_INDEX_DEF KEY_MAPPING_TABLE[KEYPAD_ROW_SIZE][KEYPAD_COLUMN_SIZE] =
// {
//     {VK_POWER,           VK_VOICE,           VK_HOME,        VK_MENU},
//     {VK_VOLUME_UP,       VK_VOLUME_DOWN,     VK_ENTER,       VK_EXIT},
//     {VK_LEFT,            VK_RIGHT,           VK_UP,          VK_DOWN},
//     {MM_VolumeIncrement, MM_VolumeDecrement, VK_TV_POWER,    VK_TV_SIGNAL},
//     {MM_AC_Home,         MM_AC_Back,         MM_AC_Forward,  MM_AC_Stop},
// };

// /* BLE HID code table definition */
// const T_KEY_CODE_DEF KEY_CODE_TABLE[KEY_CODE_TABLE_SIZE] =
// {
//     /* key_type,            ir_key_code,     hid usage page,         hid_usage_id */
//     {KEY_TYPE_NONE,         0x00,           HID_USAGE_UNDEFINED,     0x00},  /* VK_NONE */
//     {KEY_TYPE_BLE_OR_IR,    0x18,           HID_USAGE_KEY,           0x66},  /* VK_POWER */
//     {KEY_TYPE_BLE_OR_IR,    0x08,           HID_USAGE_KEY,           0x4B},  /* VK_PAGE_UP */
//     {KEY_TYPE_BLE_OR_IR,    0x09,           HID_USAGE_KEY,           0x4E},  /* VK_PAGE_DOWN */
//     {KEY_TYPE_BLE_OR_IR,    0x56,           HID_USAGE_KEY,           0x76},  /* VK_MENU */
//     {KEY_TYPE_BLE_OR_IR,    0x14,           HID_USAGE_KEY,           0x4A},  /* VK_HOME */
//     {KEY_TYPE_BLE_ONLY,     0x3E,           HID_USAGE_KEY,           0x3E},  /* VK_VOICE */
//     {KEY_TYPE_BLE_OR_IR,    0x4C,           HID_USAGE_KEY,           0x28},  /* VK_ENTER */
//     {KEY_TYPE_BLE_OR_IR,    0x57,           HID_USAGE_KEY,           0x29},  /* VK_EXIT */
//     {KEY_TYPE_BLE_OR_IR,    0x0C,           HID_USAGE_KEY,           0x50},  /* VK_LEFT */
//     {KEY_TYPE_BLE_OR_IR,    0x0E,           HID_USAGE_KEY,           0x4F},  /* VK_RIGHT */
//     {KEY_TYPE_BLE_OR_IR,    0x4D,           HID_USAGE_KEY,           0x52},  /* VK_UP */
//     {KEY_TYPE_BLE_OR_IR,    0x48,           HID_USAGE_KEY,           0x51},  /* VK_DOWN */
//     {KEY_TYPE_NONE,         0x00,           HID_USAGE_UNDEFINED,     0x00},  /* VK_MOUSE_EN */
//     {KEY_TYPE_BLE_OR_IR,    0x7F,           HID_USAGE_KEY,           0x7F},  /* VK_VOLUME_MUTE */
//     {KEY_TYPE_BLE_OR_IR,    0x49,           HID_USAGE_KEY,           0x80},  /* VK_VOLUME_UP */
//     {KEY_TYPE_BLE_OR_IR,    0x4B,           HID_USAGE_KEY,           0x81},  /* VK_VOLUME_DOWN */
//     {KEY_TYPE_BLE_ONLY,     0x3F,           HID_USAGE_KEY,           0x3F},  /* VK_VOICE_STOP */
//     {KEY_TYPE_IR_ONLY,      0x01,           HID_USAGE_KEY,           0x00},  /* VK_TV_POWER */
//     {KEY_TYPE_IR_ONLY,      0x5A,           HID_USAGE_KEY,           0x00},  /* VK_TV_SIGNAL */
// #if FEATURE_SUPPORT_MULTIMEDIA_KEYBOARD
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb5},   /* MM_ScanNext */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb6},   /* MM_ScanPrevious */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xb7},   /* MM_Stop */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xcd},   /* MM_Play_Pause */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe2},   /* MM_Mute */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe5},   /* MM_BassBoost */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe7},   /* MM_Loudness */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xe9},   /* MM_VolumeIncrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0xea},   /* MM_VolumeDecrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0152}, /* MM_BassIncrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0153}, /* MM_BassDecrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0154}, /* MM_TrebleIncrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0155}, /* MM_TrebleDecrement */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0183}, /* MM_AL_ConsumerControl */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x018a}, /* MM_AL_EmailReader */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0192}, /* MM_AL_Calculator */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0194}, /* MM_AL_LocalMachineBrowser */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0221}, /* MM_AC_Search */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0223}, /* MM_AC_Home */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0224}, /* MM_AC_Back */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0225}, /* MM_AC_Forward */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0226}, /* MM_AC_Stop */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x0227}, /* MM_AC_Refresh */
//     {KEY_TYPE_BLE_OR_IR,    0x00,           HID_USAGE_CONSUMER,          0x022a}, /* MM_AC_Bookmarks */
// #endif
// };
/*============================================================================*
 *                          Local Variables
 *============================================================================*/
T_KEY_HANDLE_GLOBAL_DATA key_handle_global_data;  /* Value to indicate the reconnection key data */

/******************************************************************
 * @brief    Get key type by key index
 * @param    uint32_t - key index
 * @return   T_KEY_TYPE_DEF - key type
 * @retval   void
 */
// T_KEY_TYPE_DEF key_handle_get_key_type_by_key_index(uint32_t key_index)
// {
//     return KEY_CODE_TABLE[key_index].key_type;
// }

/******************************************************************
 * @brief    Get hid usage page by key index
 * @param    uint32_t - key index
 * @return   uint8_t - hid usage page
 * @retval   void
 */
// uint8_t key_handle_get_hid_usage_page_by_key_index(uint32_t key_index)
// {
//     return KEY_CODE_TABLE[key_index].hid_usage_page;
// }

/******************************************************************
 * @brief    Get key usage id by key index
 * @param    uint32_t - key index
 * @return   uint32_t - hid usage id
 * @retval   void
 */
// uint32_t key_handle_get_hid_usage_id_by_key_index(uint32_t key_index)
// {
//     return KEY_CODE_TABLE[key_index].hid_usage_id;
// }

/******************************************************************
 * @brief  handle key prepare hid usage buffer
 * @param  p_cur_buf - point of current hid usage buffer
 * @param  p_prev_buf - point of previous hid usage buffer
 * @return bool - ture or false
 * @retval void
 */
// bool key_handle_prepare_hid_usage_buffer(T_KEY_INDEX_DEF key_index, T_KEY_HID_USAGES_BUFFER *p_buf)
// {
//     bool result = false;
//     uint8_t buffer_index;
//     uint8_t hid_usage_page = key_handle_get_hid_usage_page_by_key_index(key_index);
//     uint16_t hid_usage_id = key_handle_get_hid_usage_id_by_key_index(key_index);
//     T_KEY_TYPE_DEF key_type = key_handle_get_key_type_by_key_index(key_index);
//     LOG_DBG("key_handle_prepare_hid_usage_buffer\n");
//     if ((KEY_TYPE_BLE_ONLY == key_type) || (KEY_TYPE_BLE_OR_IR == key_type))
//     {
//         if (HID_USAGE_KEY == hid_usage_page)
//         {
//             if (p_buf->keyboard_usage_cnt < MAX_HID_KEYBOARD_USAGE_CNT)
//             {
//                 buffer_index = p_buf->keyboard_usage_cnt;
//                 p_buf->keyboard_usage_buffer[buffer_index] = (uint8_t)hid_usage_id;
//                 p_buf->keyboard_usage_cnt += 1;

//                 result = true;    
//             }
//             else
//             {
//                 LOG_ERR("[key_handle_prepare_hid_usage_buffer] keyboard_usage_buffer is full");
//                 result = false;
//             }
//         }
    
// #if FEATURE_SUPPORT_MULTIMEDIA_KEYBOARD
//         else if (HID_USAGE_CONSUMER == hid_usage_page)
//         {
//             if (p_buf->consumer_usage_cnt < MAX_HID_CONSUMER_USAGE_CNT)
//             {
//                 buffer_index = p_buf->consumer_usage_cnt;
//                 p_buf->consumer_usage_buffer[buffer_index] = hid_usage_id;
//                 p_buf->consumer_usage_cnt += 1;

//                 result = true;
//             }
//             else
//             {
//                 LOG_ERR("[key_handle_prepare_hid_usage_buffer] keyboard_usage_buf is full");
//                 result = false;
//             }
//         }
// #endif
//     }

//     LOG_DBG("[key_handle_prepare_hid_usage_buffer] key_index is %d, result is %d\n", key_index,
//                     result);
//     return result;
// }

/******************************************************************
 * @brief  handle key notify hid usage buffer
 * @param  p_cur_buf - point of current hid usage buffer
 * @param  p_prev_buf - point of previous hid usage buffer
 * @return bool - ture or false
 * @retval void
 */
// int key_handle_notify_hid_usage_buffer(T_KEY_HID_USAGES_BUFFER *p_cur_buf,
//                                         T_KEY_HID_USAGES_BUFFER *p_prev_buf)
// {
//     int result;
//     LOG_DBG("key_handle_notify_hid_usage_buffer\n");
//     /* check parameters and status */
//     if ((p_cur_buf == NULL) || (p_prev_buf == NULL))
//     {
//         LOG_ERR("Invalid parameters");
//         return -EPERM;
//     }

//     /* check whether notification is needed or not */
//     if (0 != memcmp(p_cur_buf->keyboard_usage_buffer, p_prev_buf->keyboard_usage_buffer,
//                     sizeof(p_cur_buf->keyboard_usage_buffer)))
//     {
//         result = hog_send_keyboard_report(p_cur_buf->keyboard_usage_buffer);

//         if (result != 0)
//         {
//             LOG_ERR("send keyboard data fail!");
//         }
//     }

// #if FEATURE_SUPPORT_MULTIMEDIA_KEYBOARD
//     if (0 != memcmp(p_cur_buf->consumer_usage_buffer, p_prev_buf->consumer_usage_buffer,
//                     sizeof(p_cur_buf->consumer_usage_buffer)))
//     {
//         result = hog_send_consumer_report(p_cur_buf->consumer_usage_buffer);
//         if (result != 0)
//         {
//             LOG_ERR("send consumer data fail!");
//         }
//     }
// #endif

//     return result;
// }

/******************************************************************
 * @brief    key handle notify hid release event
 * @param    void
 * @return   bool - true of false
 * @retval   none
 */
// bool key_handle_notify_hid_release_event(void)
// {
//     bool result = false;

//     memset(&key_handle_global_data.keyscan_fifo_data, 0, sizeof(T_KEYSCAN_FIFO_DATA));
//     memset(&key_handle_global_data.current_hid_usage_buf, 0, sizeof(T_KEY_HID_USAGES_BUFFER));

//     if (0 == key_handle_notify_hid_usage_buffer(&key_handle_global_data.current_hid_usage_buf,
//                                                    &key_handle_global_data.prev_hid_usage_buf))
//     {
//         memcpy(&key_handle_global_data.prev_hid_usage_buf, &key_handle_global_data.current_hid_usage_buf,
//                sizeof(T_KEY_HID_USAGES_BUFFER));
//         result = true;
//     }

//     return result;
// }

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
 * @brief    notify hid key event by index
 * @param    uint32_t - key index
 * @return   int - err code
 * @retval   void
 */
// bool key_handle_notify_hid_key_event_by_index(uint32_t key_index)
// {
//     bool result = false;
//     LOG_DBG("key_handle_notify_hid_key_event_by_index\n");

//     memset(&key_handle_global_data.current_hid_usage_buf, 0, sizeof(T_KEY_HID_USAGES_BUFFER));

//     if (true == key_handle_prepare_hid_usage_buffer(key_index,
//                                                     &key_handle_global_data.current_hid_usage_buf))
//     {
//         if (0 == key_handle_notify_hid_usage_buffer(&key_handle_global_data.current_hid_usage_buf,
//                                                        &key_handle_global_data.prev_hid_usage_buf))
//         {
//             memcpy(&key_handle_global_data.prev_hid_usage_buf, &key_handle_global_data.current_hid_usage_buf,
//                    sizeof(T_KEY_HID_USAGES_BUFFER));
//             result = true;
// // #if SUPPORT_VOICE_FEATURE
// //             if (key_index == VK_ENTER)
// //             {
// //                 // to do
// //             }
// // #endif
//         }
//     }

//     LOG_DBG("[key_handle_notify_hid_key_event_by_index] key_index is %d, result = %d\n",
//                     key_index, result);

//     return result;
// }

/******************************************************************
 * @brief  handle one key pressed scenario
 * @param  key index - pressed key index
 * @return none
 * @retval void
 */
void key_handle_one_key_scenario(void)
{
    T_BLE_STATUS ble_status = get_ble_status();
    switch (ble_status)
    {
    case BLE_STATUS_IDLE:
        // {
        // }
        // break;
    case BLE_STATUS_ADVERTISING:
        // {
        // }
        // break;
    case BLE_STATUS_PAIRED:
        {
            LOG_DBG("voice_handle_mic_key_pressed");
            voice_handle_mic_key_pressed();
        }
        break;
    default:
        {
        }
        break;
    }
}

/******************************************************************
 * @brief  key handle check specific key index is in keyscan FIFO or not
 * @param  key_index - key index
 * @param  p_keyscan_fifo_data - the point of keyscan fifo data
 * @return bool - true or false
 * @retval void
 */
// bool key_handle_is_key_index_in_fifo(T_KEY_INDEX_DEF key_index,
//                                      T_KEYSCAN_FIFO_DATA *p_keyscan_fifo_data)
// {
//     for (uint32_t loop_index = 0; loop_index < p_keyscan_fifo_data->len; loop_index++)
//     {
//         if (key_index ==
//             KEY_MAPPING_TABLE[p_keyscan_fifo_data->key[loop_index].row][p_keyscan_fifo_data->key[loop_index].column])
//         {
//             return true;
//         }
//     }

//     return false;
// }

void key_handle_update_kscan_fifo_data(uint32_t row, uint32_t column, bool pressed)
{
    if(pressed) {
        key_handle_global_data.keyscan_fifo_data.key[key_handle_global_data.keyscan_fifo_data.len].row = row;
        key_handle_global_data.keyscan_fifo_data.key[key_handle_global_data.keyscan_fifo_data.len].column = column;
        key_handle_global_data.keyscan_fifo_data.len++;
    } else {
        key_handle_global_data.keyscan_fifo_data.len--;
    }
}