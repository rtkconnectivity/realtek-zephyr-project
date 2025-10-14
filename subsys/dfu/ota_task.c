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
LOG_MODULE_DECLARE(OTA, CONFIG_OTA_LOG_LEVEL);
#include "log_adapter.h"

#include "gap.h"
#include "gap_msg.h"
#include "app_msg.h"
#include "os_msg.h"
#include "os_task.h"
#include "app_task.h"
#include "silent_ota_application.h"
#include "otp_config.h"
#include "dfu_service.h"

#if (AON_WDG_ENABLE == 1)
#include "rtl876x_aon_wdg.h"
#endif

/*
 *                              Macros
 */

#define APP_TASK_PRIORITY   1 /* Task priorities. */
#define APP_TASK_STACK_SIZE 512 * 4

#define MAX_NUMBER_OF_GAP_MESSAGE   0x20
#define MAX_NUMBER_OF_IO_MESSAGE    0x20
#define MAX_NUMBER_OF_EVENT_MESSAGE (MAX_NUMBER_OF_GAP_MESSAGE + MAX_NUMBER_OF_IO_MESSAGE)

/*
 *                               Variables
 */
void *app_task_handle;
void *evt_queue_handle;
void *io_queue_handle;

void app_main_task(void *p_param);
void driver_init(void);

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

void app_task_init(void)
{
    os_task_create(&app_task_handle, "app", app_main_task, 0, APP_TASK_STACK_SIZE,
                   APP_TASK_PRIORITY);
}

/**
 * @brief
 *
 *
 * @param   pvParameters
 * @return  void
 */
void app_main_task(void *p_param)
{
    uint8_t event;

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
