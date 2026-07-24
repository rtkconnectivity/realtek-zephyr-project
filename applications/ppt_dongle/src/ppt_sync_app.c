/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <trace.h>
#include "ppt_sync_app.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(ppt_dongle);

static T_PPT_SYNC_APP_PARA ppt_sync_app;

void sync_msg_reg_send_cb(sync_msg_send_cb_t cb)
{
    ppt_sync_app.msg_send_cb = cb;
}

void ppt_sync_init(sync_role_t role)
{
    APP_PRINT_INFO0("ppt_sync_init");
    memset(&ppt_sync_app, 0, sizeof(ppt_sync_app));
    sync_init(role);
#if DLPS_EN
    sync_dlps_init();
#endif
}

void ppt_sync_enable(void)
{
    APP_PRINT_INFO0("ppt_sync_enable");
    sync_enable();
}

bool ppt_check_is_bonded(void)
{
    if (SYNC_ERR_CODE_SUCCESS == sync_nvm_get_bond_info(&ppt_sync_app.bond_info))
    {
        if (ppt_sync_app.bond_info.acc.addr != 0)
        {
            return true;
        }
    }
    return false;
}

bool ppt_pair(void)
{
    DBG_DIRECT("ppt pair");
    APP_PRINT_INFO0("ppt_pair");
    sync_err_code_t ret = sync_pair();

    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        LOG_ERR("ppt_pair: fail, error code %d", ret);
        DBG_DIRECT("ppt_pair: fail, error code %d", ret);
        return false;
    }
    return true;
}

bool ppt_reconnect(void)
{
    APP_PRINT_INFO0("ppt_reconnect");
    sync_err_code_t ret = sync_nvm_get_bond_info(&ppt_sync_app.bond_info);

    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_INFO1("ppt_reconnect: get bond info fail, error code %d", ret);
        return false;
    }
    ret = sync_connect(&ppt_sync_app.bond_info);
    if (ret != SYNC_ERR_CODE_SUCCESS)
    {
        APP_PRINT_INFO1("ppt_reconnect: fail, error code %d", ret);
        return false;
    }
    return true;
}

void ppt_stop_sync(void)
{
    APP_PRINT_INFO0("ppt_stop_sync");
    sync_stop();
}

bool ppt_clear_bond_info(void)
{
    sync_err_code_t ret = sync_nvm_clear_bond_info();
    if (ret == SYNC_ERR_CODE_SUCCESS)
    {
        memset(&ppt_sync_app.bond_info, 0, sizeof(ppt_sync_app.bond_info));
        APP_PRINT_INFO0("ppt_clear_bond_info: success");
    }
    else
    {
        APP_PRINT_INFO1("ppt_clear_bond_info: fail, error code %d", ret);
        return false;
    }
    return true;
}

sync_err_code_t ppt_app_send_data(sync_msg_type_t type, uint8_t msg_retrans_count,
                                  uint8_t *data, uint16_t len)
{
    if (type == SYNC_MSG_TYPE_DYNAMIC_RETRANS)
    {
        return sync_msg_send(SYNC_MSG_TYPE_DYNAMIC_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    else if (type == SYNC_MSG_TYPE_INFINITE_RETRANS)
    {
        return sync_msg_send(SYNC_MSG_TYPE_INFINITE_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    else if (type == SYNC_MSG_TYPE_FINITE_RETRANS)
    {
        if (ppt_sync_app.msg_retrans_count != msg_retrans_count)
        {
            sync_msg_set_finite_retrans(msg_retrans_count);
            ppt_sync_app.msg_retrans_count = msg_retrans_count;
        }
        return sync_msg_send(SYNC_MSG_TYPE_FINITE_RETRANS, data, len, ppt_sync_app.msg_send_cb);
    }
    return SYNC_ERR_CODE_UNKNOWN;
}
