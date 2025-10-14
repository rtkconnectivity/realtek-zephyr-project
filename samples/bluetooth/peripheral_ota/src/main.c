/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/ots.h>

#include <silent_ota_application.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#if (CONFIG_SOC_SERIES_RTL8752H)
#include <boot.h>
#include <dfu_flash.h>
#include <app_task.h>
#include <dfu_main.h>
#endif

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] =
{
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] =
{
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

void bt_le_adv_start_preset_param(void)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

    if (err)
    {
        printk("Advertising failed to start (err %d)\n", err);
        return;
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        printk("Connection failed (err %u)\n", err);
        return;
    }

    printk("Connected\n");
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
        bt_le_adv_start_preset_param();
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) =
{
    .connected = connected,
    .disconnected = disconnected,
};

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display =
{
    .passkey_display = auth_passkey_display,
    .passkey_entry = NULL,
    .cancel = auth_cancel,
};

int main(void)
{
    int err;

    if (is_ota_support_bank_switch())
    {
        printk("Starting Bluetooth Peripheral OTA-Dual-Bank Example\n");
    }
    else
    {
        printk("Starting Bluetooth Peripheral OTA-Single-Bank Example\n");
    }

    printf("Compile at %s %s\n", __DATE__, __TIME__);
    printk("CONFIG_FLASH_LOAD_OFFSET=0x%x\n", CONFIG_FLASH_LOAD_OFFSET);

#if PRINT_MORE_INFO
    print_flash_layout();
    print_all_images_version();
#endif
    LOG_INF("dfu_get_enc_setting %d", dfu_get_enc_setting());
    dfu_main();

    err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    bt_passkey_set(232323);
    bt_conn_auth_cb_register(&auth_cb_display);

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err)
    {
        printk("Advertising failed to start (err %d)\n", err);
        return 0;
    }
    printk("Advertising successfully started\n");

    /* OTA task */
    app_task_init();

    return 0;
}

static void rtk_bt_conn_disconnect(struct bt_conn *conn, void *data)
{
    bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

void bt_le_disconnect_all(void)
{
    bt_conn_foreach(BT_CONN_TYPE_LE, rtk_bt_conn_disconnect, NULL);
}
