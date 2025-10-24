/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DFU, CONFIG_DFU_LOG_LEVEL);
#include "log_adapter.h"

#include "patch_header_check.h"
#include "rtl876x_wdg.h"
#include "flash_nor_device.h"
#include "dfu_flash.h"
#include "dfu_api.h"
#include "dfu_service.h"
#include "otp.h"
#include "app_msg.h"
#include "gap.h"
#include "gap_msg.h"
#include "profile_server.h"
#include "os_timer.h"
#include "otp.h"
#include "dfu_api.h"
#include "dfu_service.h"
#include "ota_service.h"
#include "dfu_main.h"
#include "dfu_flash.h"
#include "dfu_application.h"
#include "dfu_task.h"
#include "flash_nor_device.h"

/*============================================================================*
 *                               Variables
 *============================================================================*/

T_GAP_DEV_STATE dfu_gap_cur_state = {0, 0, 0, 0};
T_GAP_CONN_STATE dfu_gap_conn_state = GAP_CONN_STATE_DISCONNECTED;
static bool rtk_dfu_active_reset_pending = false;
static uint8_t rtk_active_reset_mode = 0;

/*============================================================================*
 *                        Extern Declarations
 *============================================================================*/

void dfu_peripheral_handle_gap_msg(T_IO_MSG *p_gap_msg);

/******************************************************************
 * @fn          app_profile_callback
 * @brief      All the bt profile callbacks are handled in this function.
 *                Then the event handling function shall be called according to the serviceID
 *                of BEE_IO_MSG.
 *
 * @param    serviceID  -  service id of profile
 * @param    pData  - pointer to callback data
 * @return     void
 */

T_APP_RESULT dfu_profile_callback(T_SERVER_ID service_id, void *p_data)
{
    T_APP_RESULT app_result = APP_RESULT_SUCCESS;

    T_DFU_CALLBACK_DATA *p_dfu_cb_data = (T_DFU_CALLBACK_DATA *)p_data;
    switch (p_dfu_cb_data->msg_type)
    {
    case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION:
        {
            if (p_dfu_cb_data->msg_data.notification_indification_index == DFU_NOTIFY_ENABLE)
            {
                DFU_PRINT_INFO0("dfu notification enable");
            }
            else if (p_dfu_cb_data->msg_data.notification_indification_index ==
                     DFU_NOTIFY_DISABLE)
            {
                DFU_PRINT_INFO0("dfu notification disable");
            }
        } break;
    case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE:
        {
            uint8_t dfu_write_opcode = p_dfu_cb_data->msg_data.write.opcode;
            if (DFU_WRITE_ATTR_EXIT == dfu_write_opcode)
            {
                if (p_dfu_cb_data->msg_data.write.write_attrib_index ==
                    INDEX_DFU_CONTROL_POINT_CHAR_VALUE)
                {
                    uint8_t control_point_opcode =
                        *p_dfu_cb_data->msg_data.write.p_value;
                    switch (control_point_opcode)
                    {
                    case DFU_OPCODE_VALID_FW:
                        {
                            T_IO_MSG dfu_valid_fw_msg;
                            dfu_valid_fw_msg.type = IO_MSG_TYPE_DFU_VALID_FW;
                            dfu_valid_fw_msg.u.buf = p_dfu_cb_data->conn;
                            if (app_send_msg_to_dfutask(&dfu_valid_fw_msg) == false)
                            {
                                LOG_ERR("DFU send Valid FW msg fail!");
                            }
                        } break;
                    case DFU_OPCODE_ACTIVE_IMAGE_RESET:
                        {
                            DFU_PRINT_INFO1(
                                "DFU_OPCODE_ACTIVE_IMAGE_RESET cmd length=%d",
                                p_dfu_cb_data->msg_data.write.length);
                            if (DFU_LENGTH_ACTIVE_IMAGE_RESET_TO_OTA_MODE ==
                                p_dfu_cb_data->msg_data.write.length)
                            {
                                /*Optional, 0:Reset to Normal Mode, 1: Reset to OTA
                                 * mode*/
                                rtk_active_reset_mode = *(
                                                            p_dfu_cb_data->msg_data.write.p_value + 1);
                            }
#if (ENABLE_AUTO_BANK_SWITCH == 1)
                            if (is_ota_support_bank_switch())
                            {
                                uint32_t ota_addr;
                                ota_addr = get_header_addr_by_img_id(OTA);
                                DFU_PRINT_INFO1("DFU_OPCODE_ACTIVE_IMAGE_RESET: "
                                                "Bank switch erase ota_addr=0x%x",
                                                ota_addr);
                                unlock_flash_bp_all();
                                dfu_flash_erase_sector(ota_addr);
                                lock_flash_bp();
                            }
#endif

                            /* le_disconnect(0); */
                            extern void bt_le_disconnect_all(void);
                            bt_le_disconnect_all();
                            rtk_dfu_active_reset_pending = true;
                        } break;
                    default:
                        break;
                    }
                }
            }
            else if (DFU_WRITE_START == dfu_write_opcode)
            {
                T_IMG_CTRL_HEADER_FORMAT *p_header =
                    (T_IMG_CTRL_HEADER_FORMAT *)p_dfu_cb_data->msg_data.write.p_value;
                if (p_header->image_id >= OTA && p_header->image_id < IMAGE_MAX &&
                    p_header->payload_len < flash_nor_get_bank_size(FLASH_OTA_BANK_0))
                {
                    uint32_t total_period =
                        timeout_value_total *
                        ((p_header->payload_len + 1024) / 102400 + 1);
                    os_timer_restart(&total_timer_handle, total_period);
                    uint32_t transfer_period =
                        timeout_value_image_transfer *
                        ((p_header->payload_len + 1024) / 102400 + 1);
                    os_timer_restart(&image_transfer_timer_handle, transfer_period);
                    APP_PRINT_ERROR2(
                        "[Normal OTA] restart timer, total=%dms, transfer=%dms!",
                        total_period, transfer_period);
                }
            }
            else if (DFU_WRITE_FAIL == dfu_write_opcode)
            {
                APP_PRINT_ERROR2("DFU FAIL! reason=%d",
                                 *p_dfu_cb_data->msg_data.write.p_value);
            }
            else
            {
            }
        } break;
    default:
        break;
    }

    return app_result;
}

/*
 * @fn          app_handle_io_msg
 * @brief      All the application events are pre-handled in this function.
 *                All the IO MSGs are sent to this function, Then the event handling function
 *                shall be called according to the MSG type.
 *
 * @param    io_msg  - bee io msg data
 * @return     void
 */
void dfu_handle_io_msg(T_IO_MSG io_msg)
{
    uint16_t msg_type = io_msg.type;

    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
            dfu_peripheral_handle_gap_msg(&io_msg);
        } break;
    case IO_MSG_TYPE_DFU_VALID_FW:
        {
            LOG_INF("IO_MSG_TYPE_DFU_VALID_FW");
            dfu_service_handle_valid_fw(io_msg.u.buf);
        } break;
    default:
        break;
    }
}
