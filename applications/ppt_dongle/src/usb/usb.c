/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>

#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include "trace.h"
#include <zephyr/drivers/gpio.h>

LOG_MODULE_DECLARE(ppt_dongle);

static enum usb_dc_status_code usb_status = USB_DC_UNKNOWN;

void usb_status_cb(enum usb_dc_status_code status, const uint8_t *params) {

    LOG_DBG("usb status cb: usb status is %d", status);
    if (status == USB_DC_SOF) {
        return;
    }

    usb_status = status;
};

int ppt_dongle_usb_init(void) {
    int usb_enable_ret;
    int usb_disable_ret;

    usb_enable_ret = usb_enable(usb_status_cb);

    if (usb_enable_ret != 0) {
        LOG_ERR("Unable to enable USB ,err = %d", usb_enable_ret);
        return -EINVAL;
    }

    return 0;
}
int ppt_dongle_usb_deinit(void) {
    int usb_enable_ret;

    usb_enable_ret = usb_disable();

    if (usb_enable_ret != 0) {
        LOG_ERR("Unable to disable USB");
        return -EINVAL;
    }

    return 0;
}

SYS_INIT(ppt_dongle_usb_init, APPLICATION, 50);
