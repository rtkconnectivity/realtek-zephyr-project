/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB device_next context for claude_micro (UAC2 mic + HID).
 *
 * Full speed only. VID/PID and strings keep the values of the legacy stack
 * (CONFIG_USB_DEVICE_VID/PID defaults).
 *
 * Class ops (usbd_uac2_set_ops / hid_device_register) must be registered
 * before app_usbd_init() is called.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

#define APP_USB_VID 0x2FE3
#define APP_USB_PID 0x0100

USBD_DEVICE_DEFINE(app_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   APP_USB_VID, APP_USB_PID);

USBD_DESC_LANG_DEFINE(app_lang);
USBD_DESC_MANUFACTURER_DEFINE(app_mfr, "ZEPHYR");
USBD_DESC_PRODUCT_DEFINE(app_product, "USB-DEV");
USBD_DESC_STRING_DEFINE(app_sn, "0123456789ABCDEF", USBD_DUT_STRING_SERIAL_NUMBER);

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
/* bmAttributes = 0 (bus powered, no remote wakeup), 250 mA */
USBD_CONFIGURATION_DEFINE(app_fs_config, 0, 125, &fs_cfg_desc);

static const char *const blocklist[] = {
	"dfu_dfu",
	NULL,
};

static void app_usbd_msg_cb(struct usbd_context *const ctx,
			    const struct usbd_msg *const msg)
{
	LOG_INF("USBD msg: %s", usbd_msg_type_string(msg->type));

	if (!usbd_can_detect_vbus(ctx)) {
		return;
	}

	if (msg->type == USBD_MSG_VBUS_READY) {
		if (usbd_enable(ctx)) {
			LOG_ERR("failed to enable USB device");
		}
	}

	if (msg->type == USBD_MSG_VBUS_REMOVED) {
		if (usbd_disable(ctx)) {
			LOG_ERR("failed to disable USB device");
		}
	}
}

int app_usbd_init(void)
{
	int err;

	err = usbd_add_descriptor(&app_usbd, &app_lang);
	if (err) {
		LOG_ERR("failed to add language descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&app_usbd, &app_mfr);
	if (err) {
		LOG_ERR("failed to add manufacturer descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&app_usbd, &app_product);
	if (err) {
		LOG_ERR("failed to add product descriptor (%d)", err);
		return err;
	}

	err = usbd_add_descriptor(&app_usbd, &app_sn);
	if (err) {
		LOG_ERR("failed to add serial number descriptor (%d)", err);
		return err;
	}

	err = usbd_add_configuration(&app_usbd, USBD_SPEED_FS, &app_fs_config);
	if (err) {
		LOG_ERR("failed to add FS configuration (%d)", err);
		return err;
	}

	err = usbd_register_all_classes(&app_usbd, USBD_SPEED_FS, 1, blocklist);
	if (err) {
		LOG_ERR("failed to register FS classes (%d)", err);
		return err;
	}

	/* IAD is used by UAC2 */
	usbd_device_set_code_triple(&app_usbd, USBD_SPEED_FS,
				    USB_BCC_MISCELLANEOUS, 0x02, 0x01);

	err = usbd_msg_register_cb(&app_usbd, app_usbd_msg_cb);
	if (err) {
		LOG_ERR("failed to register message callback (%d)", err);
		return err;
	}

	err = usbd_init(&app_usbd);
	if (err) {
		LOG_ERR("failed to initialize USB device (%d)", err);
		return err;
	}

	if (!usbd_can_detect_vbus(&app_usbd)) {
		err = usbd_enable(&app_usbd);
		if (err) {
			LOG_ERR("failed to enable USB device (%d)", err);
			return err;
		}
	}

	LOG_INF("usbd init done (VID 0x%04x PID 0x%04x, FS)", APP_USB_VID, APP_USB_PID);
	return 0;
}
