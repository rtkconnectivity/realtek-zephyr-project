/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <ble/ble.h>

T_BLE_STATUS ble_status = 0;

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0x80, 0x01),
	BT_DATA_BYTES(BT_DATA_UUID16_SOME,
		      BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL), /* HID Service */
		      BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)), /* Battery Service */
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static struct ble_profile profiles[BLE_PROFILE_COUNT];
static struct bt_le_conn_param *conn_param = BT_LE_MY_CONN_PARAM;

static void adv_timer_callback(struct k_timer *timer);
static void adv_work_handler(struct k_work *item);
K_WORK_DEFINE(adv_work, adv_work_handler);
K_TIMER_DEFINE(adv_timer, adv_timer_callback, NULL);

static void adv_timer_callback(struct k_timer *timer) {
	/* stop adv */
	k_work_submit(&adv_work);
}

static void adv_work_handler(struct k_work *item) {
	LOG_DBG("stop adv");
	bt_le_adv_stop();
}

static int update_connection_parameters(void)
{
	int err;
	struct bt_conn *conn = destination_connection();
	err = bt_conn_le_param_update(conn, conn_param);
	if (err) {
		LOG_ERR("Cannot update conneciton parameter (err: %d)", err);
		return err;
	}
	LOG_INF("Connection parameters update requested");
	return err;
}

bt_addr_le_t *ble_active_profile_addr(void) { 
    return &profiles[0].peer; }

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t *addr = bt_conn_get_dst(conn);

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	memcpy(&profiles[0].peer, addr, sizeof(bt_addr_le_t));

	if (err) {
		LOG_DBG("Failed to connect to %s (%u)", addr_str, err);
		return;
	}

	LOG_DBG("Connected %s", addr_str);
	ble_status = BLE_STATUS_PAIRED;
	k_timer_stop(&adv_timer);

#if defined(CONFIG_BT_SMP)
	if (bt_conn_set_security(conn, BT_SECURITY_L2)) {
		LOG_DBG("Failed to set security");
	}
#endif
	/* Proactively update conn parameters as rcu expected */
	// update_connection_parameters();
	// update_dle_params();
}

void update_dle_params(void)
{
	struct bt_conn_le_data_len_param dle_params = {
		.tx_max_len = 251,
		.tx_max_time = 2120 // tx max time: 2.12ms
	};
	struct bt_conn *conn = destination_connection();
	int err = bt_conn_le_data_len_update(conn, &dle_params);
	if (err) {
		LOG_ERR("set dle params error (err %d)", err);
	}
}

void handle_repairing(void)
{
	/* This feature is not stable currently */
	bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);

	int err = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	k_timer_start(&adv_timer, K_SECONDS(PAIRING_ADV_TIMEOUT), K_NO_WAIT);
	if (err) {
		LOG_DBG("Advertising failed to start (err %d)", err);
		ble_status = BLE_STATUS_IDLE;
		return;
	}
	LOG_DBG("Advertising successfully started");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("Disconnected from %s (reason 0x%02x)", addr, reason);
	ble_status = BLE_STATUS_IDLE;
}

#if defined(CONFIG_BT_SMP)
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_DBG("Security changed: %s level %u", addr, level);
	} else {
		LOG_DBG("Security failed: %s level %u err %d", addr, level,
		       err);
		ble_status = BLE_STATUS_IDLE;
	}
}
#endif

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	LOG_INF("Connection parameters update request received.\n");
	LOG_INF("Minimum interval: %d, Maximum interval: %d\n",
	       param->interval_min, param->interval_max);
	LOG_INF("Latency: %d, Timeout: %d\n", param->latency, param->timeout);

	return true;
}

static void le_data_length_updated(struct bt_conn *conn,
				   struct bt_conn_le_data_len_info *info)
{
	LOG_INF("LE data len updated: TX (len: %d time: %d)"
	       " RX (len: %d time: %d)\n", info->tx_max_len,
	       info->tx_max_time, info->rx_max_len, info->rx_max_time);
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
				 uint16_t latency, uint16_t timeout)
{
	LOG_INF("LE param updated: conn interval %d(ms), latency %d, timeout %d",
			interval, latency, timeout);
}

T_BLE_STATUS get_ble_status(void)
{
	return ble_status;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#if defined(CONFIG_BT_SMP)
	.security_changed = security_changed,
#endif
	.le_param_updated = le_param_updated,
	.le_param_req = le_param_req,
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	.le_data_len_updated = le_data_length_updated
#endif
};

static void bt_ready(int err)
{
	if (err) {
		LOG_DBG("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_DBG("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	LOG_DBG("start adv pairing");
	err = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	k_timer_start(&adv_timer, K_SECONDS(PAIRING_ADV_TIMEOUT), K_NO_WAIT);

	if (err) {
		LOG_DBG("Advertising failed to start (err %d)", err);
		ble_status = BLE_STATUS_IDLE;
		return;
	}

	LOG_DBG("Advertising successfully started");
	// ble_status = BLE_STATUS_ADVERTISING;
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("Passkey for %s: %06u", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	printk("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
}

static struct bt_gatt_cb gatt_callbacks = {.att_mtu_updated = mtu_updated};

int ble_init(void)
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		LOG_DBG("Bluetooth init failed (err %d)", err);
		return 0;
	}
	bt_gatt_cb_register(&gatt_callbacks);
	return 0;
}
