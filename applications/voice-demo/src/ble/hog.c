/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/settings/settings.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <key_handle.h>
#include <ble/ble.h>
#include <ble/hog.h>
#include <ble/hid.h>
#include "trace.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

enum {
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
    uint16_t version; /* version number of base USB HID Specification */
    uint8_t code;     /* country HID Device hardware is localized for. */
    uint8_t flags;
} __packed;

struct hids_report {
    uint8_t id;   /* report id */
    uint8_t type; /* report type */
} __packed;

static struct hids_info info = {
    .version = 0x0000,
    .code = 0x00,
    .flags = HIDS_NORMALLY_CONNECTABLE | HIDS_REMOTE_WAKE,
};

enum {
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
    HIDS_FEATURE = 0x03,
};

static struct hids_report input = {
    .id = HID_REPORT_ID_KEYBOARD,
    .type = HIDS_INPUT,
};

static struct hids_report consumer_input = {
    .id = HID_REPORT_ID_CONSUMER,
    .type = HIDS_INPUT,
};

static struct hids_report voice_input = {
    .id = HID_REPORT_ID_VOICE,
    .type = HIDS_INPUT,
};

static bool host_requests_notification = false;
static uint8_t ctrl_point;

static ssize_t read_info(struct bt_conn *conn,
			  const struct bt_gatt_attr *attr, void *buf,
			  uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_info));
}

static ssize_t read_report_map(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_report_desc,
				 sizeof(hid_report_desc));
}

static ssize_t read_report(struct bt_conn *conn,
			   const struct bt_gatt_attr *attr, void *buf,
			   uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
				 sizeof(struct hids_report));
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	host_requests_notification = (value == BT_GATT_CCC_NOTIFY) ? true : false;
}
static ssize_t read_input_report(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr, void *buf,
				 uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t read_consumer_input_report(struct bt_conn *conn,
                                               const struct bt_gatt_attr *attr, void *buf,
                                               uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t read_voice_input_report(struct bt_conn *conn,
                                               const struct bt_gatt_attr *attr, void *buf,
                                               uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_ctrl_point(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	uint8_t *value = attr->user_data;

	if (offset + len > sizeof(ctrl_point)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	memcpy(value + offset, buf, len);

	return len;
}
/* HID Service Declaration */
BT_GATT_SERVICE_DEFINE(hog_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_info,
                           NULL, &info),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
                           read_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_report,
                       NULL, &input),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_consumer_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_report,
                       NULL, &consumer_input),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_voice_input_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_report,
                       NULL, &voice_input),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point),
);

K_THREAD_STACK_DEFINE(hog_q_stack, 1024);

struct k_work_q hog_work_q;

K_MSGQ_DEFINE(hog_keyboard_msgq, MAX_HID_KEYBOARD_USAGE_CNT,
              6, 4);

struct bt_conn *destination_connection(void) {
    struct bt_conn *conn;
    bt_addr_le_t *addr = ble_active_profile_addr();
    // char addr_str[BT_ADDR_LE_STR_LEN];
    // bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    // LOG_DBG("Loaded %s address for active profile",addr_str);

    LOG_DBG("Address pointer %p", addr);
    if (!bt_addr_le_cmp(addr, BT_ADDR_LE_ANY)) {
        LOG_WRN("Not sending, no active address for current profile");
        return NULL;
    } else if ((conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, addr)) == NULL) {
        LOG_WRN("Not sending, not connected to active profile");
        return NULL;
    }

    return conn;
}

K_MSGQ_DEFINE(hog_voice_msgq, MAX_HID_VOICE_USAGE_CNT, 10, 4);

void send_voice_report_callback(struct k_work *work) {
    uint8_t report[MAX_HID_VOICE_USAGE_CNT] = {0};
    while (k_msgq_get(&hog_voice_msgq, report, K_NO_WAIT) == 0) {
        LOG_HEXDUMP_DBG(report, sizeof(report), "send voice data");
        struct bt_conn *conn = destination_connection();
        if (conn == NULL) {
            return;
        }
        int err = bt_gatt_notify(NULL, &hog_svc.attrs[13], report, sizeof(report));
        if (err) {
            LOG_DBG("Error notifying %d", err);
        }
    }
}

K_WORK_DEFINE(hog_voice_work, send_voice_report_callback);

int hog_send_voice_report(uint8_t *report)
{
    int err = k_msgq_put(&hog_voice_msgq, report, K_MSEC(15));
    if (err) {
        switch (err) {
        case -EAGAIN: {
            LOG_WRN("voice message queue full, popping first message and queueing again");
            uint8_t discarded_report[120];
            k_msgq_get(&hog_voice_msgq, discarded_report, K_NO_WAIT);
            return hog_send_voice_report(report);
        }
        default:
            LOG_WRN("Failed to queue voice report to send (%d)", err);
            return err;
        }
    }

    k_work_submit_to_queue(&hog_work_q, &hog_voice_work);

    return 0;
}

void send_keyboard_report_callback(struct k_work *work) {
    uint8_t report[MAX_HID_KEYBOARD_USAGE_CNT] = {0};
    while (k_msgq_get(&hog_keyboard_msgq, report, K_NO_WAIT) == 0) {
        LOG_HEXDUMP_DBG(report, sizeof(report),"send keyboard data");
        struct bt_conn *conn = destination_connection();
        if (conn == NULL) {
            return;
        }
        int err = bt_gatt_notify(NULL, &hog_svc.attrs[5], report, sizeof(report));
        if (err) {
            LOG_DBG("Error notifying %d", err);
        }
    }
}

K_WORK_DEFINE(hog_keyboard_work, send_keyboard_report_callback);

int hog_send_keyboard_report(uint8_t *report) {
    int err = k_msgq_put(&hog_keyboard_msgq, report, K_MSEC(100));
    if (err) {
        switch (err) {
        case -EAGAIN: {
            LOG_WRN("Keyboard message queue full, popping first message and queueing again");
            uint8_t discarded_report[8];
            k_msgq_get(&hog_keyboard_msgq, discarded_report, K_NO_WAIT);
            return hog_send_keyboard_report(report);
        }
        default:
            LOG_WRN("Failed to queue keyboard report to send (%d)", err);
            return err;
        }
    }

    k_work_submit_to_queue(&hog_work_q, &hog_keyboard_work);

    return 0;
};

K_MSGQ_DEFINE(hog_consumer_msgq, 2*MAX_HID_CONSUMER_USAGE_CNT,
              1, 4);

void send_consumer_report_callback(struct k_work *work) {
    uint16_t report[MAX_HID_CONSUMER_USAGE_CNT];

    while (k_msgq_get(&hog_consumer_msgq, report, K_NO_WAIT) == 0) {
        struct bt_conn *conn = destination_connection();
        if (conn == NULL) {
            return;
        }
        LOG_HEXDUMP_DBG(report, sizeof(report),"send consumer data");
        int err = bt_gatt_notify(conn, &hog_svc.attrs[9],
				       report, sizeof(report));
        if (err) {
            LOG_DBG("Error notifying %d", err);
        }
    }
};

K_WORK_DEFINE(hog_consumer_work, send_consumer_report_callback);

int hog_send_consumer_report(uint16_t *report) {
    int err = k_msgq_put(&hog_consumer_msgq, report, K_MSEC(100));
    if (err) {
        switch (err) {
        case -EAGAIN: {
            LOG_WRN("Consumer message queue full, popping first message and queueing again");
            uint16_t discarded_report[3];
            k_msgq_get(&hog_consumer_msgq, discarded_report, K_NO_WAIT);
            return hog_send_consumer_report(report);
        }
        default:
            LOG_WRN("Failed to queue consumer report to send (%d)", err);
            return err;
        }
    }

    k_work_submit_to_queue(&hog_work_q, &hog_consumer_work);

    return 0;
};

static int hog_init(void) {
    static const struct k_work_queue_config queue_config = {.name = "HID Over GATT Send Work"};
    k_work_queue_start(&hog_work_q, hog_q_stack, K_THREAD_STACK_SIZEOF(hog_q_stack),
                       5, &queue_config);

    return 0;
}

SYS_INIT(hog_init, APPLICATION, 50);
