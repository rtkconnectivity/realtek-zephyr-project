#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(silent_ota, LOG_LEVEL_DBG);

#include "app_msg.h"
#include "app_task.h"
#include "gap.h"
#include "gap_msg.h"
#include "dfu_service.h"
#include "log_adapter.h"
#include "silent_ota_application.h"
#include "dfu_flash.h"

extern void bt_le_adv_start_preset_param(void);

/*
 *                              Macros
 */
/* convert img version bit map to deirectly copmare version value */
#define IMG_VER_TO_LE_VALUE(p)                                                                     \
    (((p & 0xf) << 28) + (((p & 0xff0) >> 4) << 20) + (((p & 0x07fff000) >> 12) << 5) +        \
     ((p & 0xf8000000) >> 27))
#define OTA_IMG_VER_TO_LE_VALUE(p)                                                                 \
    (((p & 0xff) << 24) + (((p >> 8) & 0xff) << 16) + (((p >> 16) & 0xff) << 8) +              \
     ((p >> 24) & 0xff))

/*
 *                              external Variables
 */
extern T_SERVER_ID g_bas_service_id;
extern T_SERVER_ID g_dis_service_id;
extern T_SERVER_ID g_ota_service_id;
extern T_SERVER_ID g_dfu_service_id;

uint16_t g_bat_vol;
uint16_t g_bas_battery_level;

/*
 *                              Local Variables
 */
T_GAP_DEV_STATE gap_dev_state = {0, 0, 0, 0};                  /**< GAP device state */
T_GAP_CONN_STATE gap_conn_state = GAP_CONN_STATE_DISCONNECTED; /**< GAP connection state */

bool switch_to_ota_mode_pending;
bool dfu_active_rst_pending;
LantencyStatus g_lantency_status = LANTENCY_OFF;

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
                            extern void bt_le_disconnect_all(void);
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

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
    LOG_INF("init state %d, adv state %d, cause 0x%x",
            new_state.gap_init_state, new_state.gap_adv_state, cause);
    if (gap_dev_state.gap_init_state != new_state.gap_init_state)
    {
        if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY)
        {
            LOG_INF("GAP stack ready");
            /*stack ready*/
            bt_le_adv_start_preset_param();
        }
    }

    if (gap_dev_state.gap_adv_state != new_state.gap_adv_state)
    {
        if (new_state.gap_adv_state == GAP_ADV_STATE_IDLE)
        {
            if (new_state.gap_adv_sub_state == GAP_ADV_TO_IDLE_CAUSE_CONN)
            {
                LOG_INF("GAP adv stopped: because connection created");
            }
            else
            {
                LOG_INF("GAP adv stopped");
            }
        }
        else if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)
        {

            LOG_INF("GAP adv start");
        }
    }

    gap_dev_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_STATE_CHANGE
 * @note     All the gap conn state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New gap connection state
 * @param[in] disc_cause Use this cause when new_state is GAP_CONN_STATE_DISCONNECTED
 * @return   void
 */
void app_handle_conn_state_evt(struct bt_conn *conn, T_GAP_CONN_STATE new_state,
                               uint16_t disc_cause)
{
    LOG_INF("conn %x old_state %d new_state %d, disc_cause 0x%x",
            (uint32_t)conn, gap_conn_state, new_state, disc_cause);
    switch (new_state)
    {
    case GAP_CONN_STATE_DISCONNECTED:
        {
            if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE)) &&
                (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE)))
            {
                LOG_ERR("connection lost cause 0x%x",
                        disc_cause);
            }

            if (switch_to_ota_mode_pending)
            {
#if (SUPPORT_NORMAL_OTA == 1)
                switch_to_ota_mode_pending = false;
                extern void dfu_switch_to_ota_mode(void);
                dfu_switch_to_ota_mode();
#endif
            }
            else
            {
                if (dfu_active_rst_pending)
                {
                    dfu_active_rst_pending = false;
                    /* unlock_flash_bp_all(); */
                    dfu_fw_reboot(true);
                }
                else
                {
                    bt_le_adv_start_preset_param();
                }
            }

            /* bt_le_adv_start_preset_param(); */
        } break;

    case GAP_CONN_STATE_CONNECTED:
        {
            uint16_t conn_interval;
            uint16_t conn_latency;
            uint16_t conn_supervision_timeout;
            /* uint8_t remote_bd[6]; */
            T_GAP_REMOTE_ADDR_TYPE remote_bd_type;
            struct bt_conn_info info;

            bt_conn_get_info(conn, &info);
            conn_interval = info.le.interval;
            conn_latency = info.le.latency;
            conn_supervision_timeout = info.le.timeout;

            const bt_addr_le_t *remote_addr = bt_conn_get_dst(conn);

            remote_bd_type = remote_addr->type;
            LOG_INF("GAP_CONN_STATE_CONNECTED: remote_addr_type %d, conn_interval "
                    "0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
                    remote_bd_type, conn_interval, conn_latency, conn_supervision_timeout);
            LOG_HEXDUMP_INF(remote_addr->a.val, BT_ADDR_SIZE, "remote_bd");

            g_dfu_para.dfu_conn_para_update_in_progress = false;
            g_dfu_para.dfu_conn_interval = conn_interval;
            g_dfu_para.dfu_conn_lantency = conn_latency;

#if F_BT_LE_5_0_SET_PHY_SUPPORT
            uint8_t tx_phy;
            uint8_t rx_phy;

            le_get_conn_param(GAP_PARAM_CONN_RX_PHY_TYPE, &rx_phy, conn_id);
            le_get_conn_param(GAP_PARAM_CONN_TX_PHY_TYPE, &tx_phy, conn_id);
            LOG_INF("GAP_CONN_STATE_CONNECTED: tx_phy %d, rx_phy %d", tx_phy, rx_phy);

            uint8_t all_phys;
            uint8_t tx_phys;
            uint8_t rx_phys;

            T_GAP_PHYS_OPTIONS phy_options = GAP_PHYS_OPTIONS_CODED_PREFER_S8;

            all_phys = GAP_PHYS_PREFER_ALL;
            tx_phys = GAP_PHYS_PREFER_2M_BIT;
            rx_phys = GAP_PHYS_PREFER_2M_BIT;
            le_set_phy(conn_id, all_phys, tx_phys, rx_phys, phy_options);
#endif

#if (ENABLE_SLAVE_REQUEST_UPDATE_COON_PARA == 1)
            T_GAP_CAUSE update_conn_para_cause = le_update_conn_param(
                                                     0, CONNECT_INTERVAL_MIN, CONNECT_INTERVAL_MAX, CONNECT_LATENCY,
                                                     SUPERVISION_TIMEOUT, 2 * (CONNECT_INTERVAL_MIN - 1),
                                                     2 * (CONNECT_INTERVAL_MAX - 1));
            if (GAP_CAUSE_SUCCESS != update_conn_para_cause)
            {
                DFU_PRINT_INFO1("le_update_conn_param: send HCI command failed! cause=%d",
                                update_conn_para_cause);
            }
#endif

        } break;

    default:
        break;
    }
    gap_conn_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_AUTHEN_STATE_CHANGE
 * @note     All the gap authentication state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New authentication state
 * @param[in] cause Use this cause when new_state is GAP_AUTHEN_STATE_COMPLETE
 * @return   void
 */
void app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
    LOG_INF("conn_id %d, cause 0x%x", conn_id, cause);

    switch (new_state)
    {
    case GAP_AUTHEN_STATE_STARTED:
        {
            LOG_INF("GAP_AUTHEN_STATE_STARTED");
        } break;

    case GAP_AUTHEN_STATE_COMPLETE:
        {
            if (cause == GAP_SUCCESS)
            {
                g_lantency_status = LANTENCY_UPDATING;
                LOG_INF("GAP_AUTHEN_STATE_COMPLETE "
                        "pair success");
            }
            else
            {
                LOG_INF("GAP_AUTHEN_STATE_COMPLETE "
                        "pair failed");
            }
        } break;

    default:
        {
            LOG_ERR("unknown newstate %d", new_state);
        } break;
    }
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_MTU_INFO
 * @note     This msg is used to inform APP that exchange mtu procedure is completed.
 * @param[in] conn_id Connection ID
 * @param[in] mtu_size  New mtu size
 * @return   void
 */
void app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
    LOG_INF("conn_id %d, mtu_size %d", conn_id, mtu_size);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void app_handle_conn_param_update_evt(struct bt_conn *conn, uint8_t status, uint16_t cause)
{
    switch (status)
    {
    case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS:
        {
            uint16_t conn_interval;
            uint16_t conn_slave_latency;
            uint16_t conn_supervision_timeout;
            struct bt_conn_info info;

            bt_conn_get_info(conn, &info);
            conn_interval = info.le.interval;
            conn_slave_latency = info.le.latency;
            conn_supervision_timeout = info.le.timeout;
            if (conn_slave_latency)
            {
                g_lantency_status = LANTENCY_ON;
            }

            if (g_dfu_para.dfu_conn_para_update_in_progress == true)
            {
                g_dfu_para.dfu_conn_interval = conn_interval;
                g_dfu_para.dfu_conn_lantency = conn_slave_latency;
            }
            LOG_INF("update success: interval 0x%x, "
                    "slave_latency 0x%x, supervision_timeout 0x%x",
                    conn_interval, conn_slave_latency, conn_supervision_timeout);
            dfu_notify_conn_para_update_req(conn, DFU_ARV_SUCCESS);
        } break;

    case GAP_CONN_PARAM_UPDATE_STATUS_FAIL:
        {
            LOG_ERR("update failed: cause 0x%x", cause);
            dfu_notify_conn_para_update_req(conn, DFU_ARV_FAIL_OPERATION);
        } break;

    case GAP_CONN_PARAM_UPDATE_STATUS_PENDING:
        {
            LOG_INF("update pending.");
        } break;

    default:
        break;
    }
}

T_APP_RESULT app_profile_callback(T_SERVER_ID service_id, void *p_data)
{
    T_APP_RESULT app_result = APP_RESULT_SUCCESS;

    app_result = app_dfu_srv_cb((T_DFU_CALLBACK_DATA *)p_data);

    return app_result;
}
