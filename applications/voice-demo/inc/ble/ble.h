/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/addr.h>

#define BLE_PROFILE_COUNT 1
#define BLE_PROFILE_NAME_MAX 1

#define PAIRING_ADV_TIMEOUT 60 /* 60s */
#define RECONN_ADV_TIMEOUT 3   /* 3s */
#define CONNECT_INTERVAL                0x10    /*0x10 * 1.25ms = 20ms*/
#define CONNECT_LATENCY                 49      /* 49 */
#define SUPERVISION_TIMEOUT             6000    /* 6s */
#define BT_GAP_ADV_INT_MIN              0x0020  /* 20ms */
#define BT_GAP_ADV_INT_MAX              0x0030  /* 30ms */
#define BT_LE_MY_CONN_PARAM BT_LE_CONN_PARAM(CONNECT_INTERVAL, CONNECT_INTERVAL,\
                                             CONNECT_LATENCY, SUPERVISION_TIMEOUT/10)

/** This is the rcu recommanded configurations for connectable advertisers.
 */
#define BT_LE_ADV_CONN_ONE_TIME                                                                    \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_ONE_TIME,                        \
			BT_GAP_ADV_INT_MIN, BT_GAP_ADV_INT_MAX, NULL)

typedef enum
{
    BLE_STATUS_IDLE = 0,  /* idle status */
    BLE_STATUS_ADVERTISING,  /* adversting status */
    BLE_STATUS_STOP_ADVERTISING,  /* temporary status of stop adversting */
    BLE_STATUS_CONNECTED,  /* connect status but not start paring */
    BLE_STATUS_PAIRED,  /* paired success status */
    BLE_STATUS_DISCONNECTING,  /* temporary status of disconnecting */
    BLE_STATUS_LOW_POWER,     /* low power mode*/
} T_BLE_STATUS;

struct ble_profile {
    char name[BLE_PROFILE_NAME_MAX];
    bt_addr_le_t peer;
};

typedef enum
{
    LANTENCY_ON = 0,
    LANTENCY_OFF,
} T_LATENCY_STATE;

bt_addr_le_t *ble_active_profile_addr(void);
void handle_repairing(void);
int ble_init(void);
int set_latency_status(T_LATENCY_STATE latency_status);
T_BLE_STATUS get_ble_status(void);
