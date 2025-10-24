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

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(REALTEK_OTA, CONFIG_REALTEK_OTA_LOG_LEVEL);
#include "log_adapter.h"

#include "app_msg.h"
#include "os_msg.h"
#include "os_task.h"
#include "dfu_common.h"
#include "silent_ota.h"
#include "ota_config.h"
#include "dfu_service.h"
#include "dfu_main.h"
#include "dfu_flash.h"

#if (AON_WDG_ENABLE == 1)
#include "rtl876x_aon_wdg.h"
#endif

#define APP_TASK_PRIORITY   5
#define APP_TASK_STACK_SIZE 512 * 4

#define MAX_NUMBER_OF_IO_MESSAGE    0x10
#define MAX_NUMBER_OF_EVENT_MESSAGE (MAX_NUMBER_OF_IO_MESSAGE)

static bool dfu_active_rst_pending = false;

static void rtk_bt_conn_disconnect(struct bt_conn *conn, void *data)
{
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

void bt_le_disconnect_all(void)
{
    bt_conn_foreach(BT_CONN_TYPE_LE, rtk_bt_conn_disconnect, NULL);
}

void *app_task_handle;
void *evt_queue_handle;
void *io_queue_handle;

/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void app_handle_io_msg(T_IO_MSG io_msg)
{
    uint16_t msg_type = io_msg.type;

    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
            LOG_WRN("app_handle_gap_msg haven't been supported");
            /* app_handle_gap_msg(&io_msg); */
        } break;
    case IO_MSG_TYPE_DFU_VALID_FW:
        {
            LOG_INF("IO_MSG_TYPE_DFU_VALID_FW");
            dfu_service_handle_valid_fw(io_msg.u.buf);
        } break;
#if (ROM_WATCH_DOG_ENABLE == 1)
    case IO_MSG_TYPE_RESET_WDG_TIMER:
        {
            LOG_INF("[WDG] Watch Dog Rset Timer");
            WDG_Restart();
        } break;
#endif
    default:
        break;
    }
}

/**
 * \brief    send msg queue to app task.
 *
 * \param[in]   p_handle   The handle to the message queue being peeked.
 *
 * \return           The status of the message queue peek.
 * \retval true      Message queue was peeked successfully.
 * \retval false     Message queue was failed to peek.
 */
bool app_send_msg_to_apptask(T_IO_MSG *p_msg)
{
    uint8_t event = EVENT_IO_TO_APP;

    if (os_msg_send(io_queue_handle, p_msg, 0) == false)
    {
        LOG_ERR("send_io_msg_to_app fail");
        return false;
    }
    if (os_msg_send(evt_queue_handle, &event, 0) == false)
    {
        LOG_ERR("send_evt_msg_to_app fail");
        return false;
    }
    return true;
}

static T_APP_RESULT app_dfu_srv_cb(T_DFU_CALLBACK_DATA *p_data)
{
    T_APP_RESULT app_result = APP_RESULT_SUCCESS;

    switch (p_data->msg_type)
    {
    case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION:
        {
            if (p_data->msg_data.notification_indification_index == DFU_NOTIFY_ENABLE)
            {
                LOG_INF("dfu notification enable");
            }
            else if (p_data->msg_data.notification_indification_index == DFU_NOTIFY_DISABLE)
            {
                LOG_INF("dfu notification disable");
            }
        } break;
    case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE:
        {
            uint8_t dfu_write_opcode = p_data->msg_data.write.opcode;

            if (dfu_write_opcode == DFU_WRITE_ATTR_EXIT)
            {
                if (p_data->msg_data.write.write_attrib_index ==
                    INDEX_DFU_CONTROL_POINT_CHAR_VALUE)
                {
                    uint8_t control_point_opcode = *p_data->msg_data.write.p_value;

                    switch (control_point_opcode)
                    {
                    case DFU_OPCODE_VALID_FW:
                        {
                            T_IO_MSG dfu_valid_fw_msg;

                            dfu_valid_fw_msg.type = IO_MSG_TYPE_DFU_VALID_FW;
                            /* dfu_valid_fw_msg.u.param = p_data->conn; */
                            dfu_valid_fw_msg.u.buf = p_data->conn;
                            if (app_send_msg_to_apptask(&dfu_valid_fw_msg) == false)
                            {
                                LOG_ERR("DFU send Valid FW msg fail!");
                            }
                        } break;
                    case DFU_OPCODE_ACTIVE_IMAGE_RESET:
                        {
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
                            bt_le_disconnect_all();
                            dfu_active_rst_pending = true;
                        } break;
                    default:
                        break;
                    }
                }
            }
            else if (dfu_write_opcode == DFU_WRITE_FAIL)
            {
                LOG_ERR("DFU FAIL!");
            }
            else if (dfu_write_opcode == DFU_WRITE_ATTR_ENTER)
            {
                /**
                 * application can add check conditions before start dfu procefure
                 * if check fail, return some error code except APP_RESULT_SUCCESS to exit
                 * dfu procedure app_result = APP_RESULT_REJECT; LOG_INF("exit dfu
                 * procedure");
                 */
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

T_APP_RESULT app_profile_callback(T_SERVER_ID service_id, void *p_data)
{
    T_APP_RESULT app_result = APP_RESULT_SUCCESS;

    app_result = app_dfu_srv_cb((T_DFU_CALLBACK_DATA *)p_data);

    return app_result;
}

void silent_ota_task(void *p_param)
{
    uint8_t event;

    dfu_add_service(app_profile_callback);

    printf("Compile at %s %s\n", __DATE__, __TIME__);

    LOG_INF("dfu_get_enc_setting %d", dfu_get_enc_setting());

    print_flash_layout();
    print_all_images_version();

    os_msg_queue_create(&io_queue_handle, MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&evt_queue_handle, MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

#if (ROM_WATCH_DOG_ENABLE == 1)
    extern void reset_watch_dog_timer_enable(void);
    reset_watch_dog_timer_enable();
#endif

#if (AON_WDG_ENABLE == 1)
    aon_wdg_init(1, AON_WDG_TIME_OUT_PERIOD_SECOND);
    aon_wdg_enable();
    AON_WDG_Restart();
#endif

    while (true)
    {
        if (os_msg_recv(evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            LOG_INF("os_msg_recv");
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;

                if (os_msg_recv(io_queue_handle, &io_msg, 0) == true)
                {
                    app_handle_io_msg(io_msg);
                }
            }
            else
            {
                /* gap_handle_msg(event); */
            }
        }
    }
}
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);
#if (SUPPORT_NORMAL_OTA == 1)
    printk("NORMAL_OTA is ToDo.\n");
    if (switch_to_ota_mode_pending)
    {
    }
#endif
    if (dfu_active_rst_pending)
    {
        dfu_active_rst_pending = false;
        /* unlock_flash_bp_all(); */
        dfu_fw_reboot(true);
    }
    else
    {
        /* do nothing. BT would start adv automatically.*/
    }
}

BT_CONN_CB_DEFINE(ota_ble_callbacks) = {
	.disconnected = disconnected,
};

K_THREAD_DEFINE(silent_ota_task_tid, APP_TASK_STACK_SIZE,
                silent_ota_task, NULL, NULL, NULL,
                APP_TASK_PRIORITY, 0, 0);