/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/logging/log.h>
#include <kscan.h>
#include <key_handle.h>
#include <ble/hid.h>
#include "rtl_pinmux.h"
#include "trace.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

#define KSCAN_EVENT_STATE_PRESSED 0
#define KSCAN_EVENT_STATE_RELEASED 1

static uint8_t key_pressed_num = 0;

struct kscan_event {
    uint32_t row;
    uint32_t column;
    uint32_t state;
};

struct kscan_msg_processor {
    struct k_work work;
} msg_processor;

static uint8_t key_press_num = 0;

K_MSGQ_DEFINE(kscan_msgq, sizeof(struct kscan_event), 1, 4);

static void kscan_callback(const struct device *dev, uint32_t row, uint32_t column,
                               bool pressed) {
    LOG_DBG("keyscan callback: row,col is (%d %d), pressed %d", row, column, pressed);

    key_handle_update_kscan_fifo_data(row, column, pressed);
    if(pressed) {
        key_press_num += 1;
    } else {
        key_press_num -= 1;
    }

    struct kscan_event ev = {
        .row = row,
        .column = column,
        .state = (pressed ? KSCAN_EVENT_STATE_PRESSED : KSCAN_EVENT_STATE_RELEASED)};

    k_msgq_put(&kscan_msgq, &ev, K_NO_WAIT);
    k_work_submit(&msg_processor.work);
}

void kscan_process_msgq(struct k_work *item) {
    struct kscan_event ev;
    int err;
    LOG_DBG("enter key press handler, key press num %d", key_press_num);
    while (k_msgq_get(&kscan_msgq, &ev, K_NO_WAIT) == 0) {
        bool pressed = (ev.state == KSCAN_EVENT_STATE_PRESSED);
        // T_KEY_INDEX_DEF key_index = key_handle_get_key_index_by_row_column(ev.row,ev.column);

        if(key_press_num == 0) {
            key_handle_release_event();
        } else if(key_press_num == 1) {
            key_handle_one_key_scenario();
        } else if(key_press_num >= 2) {
            // key_handle_detect_combine_keys(&key_handle_global_data.keyscan_fifo_data);
        }
    }
}

int kscan_init(const struct device *dev) {
    if (dev == NULL) {
        LOG_ERR("Failed to get the KSCAN device");
        return -EINVAL;
    }
    k_work_init(&msg_processor.work, kscan_process_msgq);

    kscan_config(dev, kscan_callback);
    kscan_enable_callback(dev);
    return 0;
}