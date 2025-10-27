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
LOG_MODULE_REGISTER(OTA, CONFIG_OTA_LOG_LEVEL);
#include "log_adapter.h"

#include "patch_header_check.h"
#include "flash_map.h"
#include "ota_service.h"
#include "otp.h"
#include "board.h"
#include "dfu_flash.h"
#include "flash_nor_device.h"

/*
 *                              Micros
 */
#define SOCV_CFG    0
#define SYS_CFG     1
#define OTA_HEADER  2
#define SECURE_BOOT 3
#define ROM_PATCH   4
#define APP_IMG     5
#define APP_DATA1   6
#define APP_DATA2   7
#define APP_DATA3   8
#define APP_DATA4   9
#define APP_DATA5   10
#define APP_DATA6   11
#define UPPERSTACK  12
#define STACK_PATCH 13

#define IMAGE_NOEXIST        0
#define IMAGE_LOCATION_BANK0 1
#define IMAGE_LOCATION_BANK1 2
#define IMAGE_FIX_BANK_EXIST 3

#define OTA_PROTOCOL_TYPE 0x0012
#define OTA_SPEC_VERSION  0x5
/*
 *                               Types
 */
typedef struct
{
    uint8_t image_num;
    uint16_t image_exist;
    uint32_t image_indicator;
} T_OTA_ACTIVE_BANK_IMG_INFO;

/*
 *                              Local Variables
 */
/**<  Function pointer used to send event to application from BWPS extended profile. */

uint8_t mac_addr[6];
uint32_t patch_version;
uint32_t patch_ext_version;
uint32_t app_version;
uint16_t protocol_type = OTA_PROTOCOL_TYPE;
static uint32_t image_version[IMAGE_MAX - OTA] = {0};
T_OTA_ACTIVE_BANK_IMG_INFO ota_active_bank_info;

extern bool switch_to_ota_mode_pending;

/**
 * @brief  get OTA active bank image info
 * @param[out]  p_ota_image_info
 * image_exist
 * bit0: ota header, bit: fsbl, bit2: patch ..., bit9: appdata6, bit10: upperstack
 * img_indicator
 * Indications for each image version.each indication use 2bit.
 * 00: image is not existed.
 * 01: image is existed in bank0,ota should update image for bank1.
 * 10: image is existed in bank1,ota should update image for bank0.
 * 11: image is standalone. ota should update image for standalone.
 *
 * bit[1:0]: Image 0
 * bit[2N+1:2N]:Image N

 * Image indicator for rtl8752h is as below:
 * Image 0   OTA Header File
 * Image 1   Secure Boot Loader Image
 * Image 2   ROM Patch Image
 * Image 3   APP Image
 * Image 4   APP Data1 File
 * Image 5   APP Data2 File
 * Image 6   APP Data3 File
 * Image 7   APP Data4 File
 * Image 8   APP Data5 File
 * Image 9   APP Data6 File
 * Image 10  Upperstack File
 * Image 11  Stack patch File
 */
static void get_ota_active_bank_image_info(T_OTA_ACTIVE_BANK_IMG_INFO *p_ota_image_info)
{
    uint32_t image_location = IMAGE_NOEXIST;
    bool enable_bank_switch = is_ota_support_bank_switch();

    p_ota_image_info->image_num = 0;

    if (enable_bank_switch)
    {
        uint32_t ota_bank0_addr = flash_nor_get_bank_addr(FLASH_OTA_BANK_0);

        if (ota_bank0_addr == get_active_ota_bank_addr())
        {
            image_location = IMAGE_LOCATION_BANK0;
        }
        else
        {
            image_location = IMAGE_LOCATION_BANK1;
        }
    }
    else
    {
        image_location = IMAGE_FIX_BANK_EXIST;
    }

    for (T_IMG_ID image_id = OTA; image_id < IMAGE_MAX; image_id++)
    {
        if (0 != get_active_bank_image_size_by_img_id(image_id))
        {
            p_ota_image_info->image_num++;
            p_ota_image_info->image_exist |= BIT(image_id - OTA);
            p_ota_image_info->image_indicator |= image_location
                                                 << ((OTA_HEADER + image_id - OTA) * 2);
        }
    }
}

static ssize_t ota_write_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                            uint16_t len, uint16_t offset, uint8_t flags)
{
    struct bt_uuid_16 *uuid = (struct bt_uuid_16 *)attr->uuid;

    LOG_INF("OTA SERVICE, UUID: 0x%x", uuid->val);

    if ((len != sizeof(uint8_t)) || (buf == NULL))
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    uint8_t *value = (uint8_t *)buf;

    /* Notify Application. */
    if (OTA_VALUE_ENTER == value[0])
    {
        /* battery level is above 60 percent */
        LOG_INF("Preparing switch into OTA mode");

        /* Flag for switch to Normal OTA mode */
        switch_to_ota_mode_pending = true;

        /* The switching to Normal OTA mode is handled in disconnect event
         * Disconnect the existed conn at here. And it will be connected afterwards
         */
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    return len;
}

static ssize_t mac_address_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                   uint16_t len, uint16_t offset)
{
    struct bt_uuid_16 *uuid = (struct bt_uuid_16 *)attr->uuid;
    bt_addr_le_t addr = {0};
    size_t count = 1;

    LOG_INF("OTA SERVICE, UUID: 0x%x", uuid->val);
    bt_id_get(&addr, &count);

    for (int i = 0; i < 6; i++)
    {
        mac_addr[i] = addr.a.val[5 - i];
        /* mac_addr[i+6] = addr.a.val[5-i]; */
    }

    const uint8_t *value = mac_addr;

    LOG_HEXDUMP_INF(value, sizeof(mac_addr), "mac_addr");

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(mac_addr));
}

static ssize_t patch_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                             uint16_t len, uint16_t offset)
{
    T_IMG_HEADER_FORMAT *p_header;
    uint32_t addr = get_header_addr_by_img_id(RomPatch);

    p_header = (T_IMG_HEADER_FORMAT *)addr;
    patch_version = p_header->git_ver.ver_info.version;
    DFU_PRINT_INFO2("ota_attr_read_cb: Patch addr=0x%x, version=0x%x", addr, patch_version);

    const char *value = (uint8_t *)&patch_version;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}

static ssize_t app_version_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                   uint16_t len, uint16_t offset)
{
    T_IMG_HEADER_FORMAT *p_header;
    uint32_t addr = get_header_addr_by_img_id(AppPatch);

    p_header = (T_IMG_HEADER_FORMAT *)addr;
    app_version = p_header->git_ver.ver_info.version;
    DFU_PRINT_INFO2("ota_attr_read_cb: APP addr=0x%x, version=0x%x", addr, app_version);

    const char *value = (uint8_t *)&app_version;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, strlen(value));
}
static ssize_t device_info_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                   uint16_t len, uint16_t offset)
{
    static T_DFU_DEVICE_INFO DeviceInfo;
    T_IMG_HEADER_FORMAT *p_header;
    uint32_t addr = get_header_addr_by_img_id(OTA);

    p_header = (T_IMG_HEADER_FORMAT *)addr;
    DeviceInfo.ic_type = IMG_IC_TYPE;
    DeviceInfo.spec_version = OTA_SPEC_VERSION;
    DeviceInfo.secure_version = p_header->ctrl_header.secure_version;
    LOG_INF("<==ota_attr_read_cb: ic_type=0x%x, spec_version=0x%x, OTA Header "
            "secure_version=0x%x",
            DeviceInfo.ic_type, DeviceInfo.spec_version, DeviceInfo.secure_version);

    DeviceInfo.ota_mode.mode_flag.buf_check_en = DFU_BUFFER_CHECK_ENABLE;
    DeviceInfo.max_buffer_size = DFU_TEMP_BUFFER_SIZE;
    DeviceInfo.ota_mode.mode_flag.aesflg = dfu_get_enc_setting();
    DeviceInfo.ota_mode.mode_flag.aesmode = 1;
    DeviceInfo.ota_mode.mode_flag.handshake = 0;
    DeviceInfo.ota_mode.mode_flag.compress_mode = SUPPORT_PUBLIC_DECODE_OTA;
    LOG_INF("<==ota_attr_read_cb: bufChkEn=%d, bufsize=0x%x, aesflg=%d, aesmode=%d, "
            "handshake=%d",
            DeviceInfo.ota_mode.mode_flag.buf_check_en, DeviceInfo.max_buffer_size,
            DeviceInfo.ota_mode.mode_flag.aesflg, DeviceInfo.ota_mode.mode_flag.aesmode,
            DeviceInfo.ota_mode.mode_flag.handshake);

    /* default disable copy app data img when bank switch */
    DeviceInfo.ota_mode.mode_flag.copy_img = 0;
    DeviceInfo.ota_mode.mode_flag.multi_img = is_ota_support_bank_switch() & 0x01;
    /**
     * prepare img_indicator, if modify here need
     * to sync with value of BLE_SERVICE_CHAR_IMAGE_VERSION_INDEX
     */
    if (is_ota_support_bank_switch())
    {
        /* dual bank must support multiple image update */
        DeviceInfo.ota_mode.mode_flag.multi_img = 1;
        uint32_t ota_bank0_addr = flash_nor_get_bank_addr(FLASH_OTA_BANK_0);

        if (ota_bank0_addr == get_active_ota_bank_addr())
        {
            /* meaningful only when multi_img = 1 */
            DeviceInfo.temp_bank_size =
                flash_nor_get_bank_size(FLASH_OTA_BANK_1) / FLASH_NOR_SECTOR_SIZE;
        }
        else
        {
            /* meaningful only when multi_img = 1 */
            DeviceInfo.temp_bank_size =
                flash_nor_get_bank_size(FLASH_OTA_BANK_0) / FLASH_NOR_SECTOR_SIZE;
        }
    }
    else
    {
#if (SUPPORT_TEMP_COMBINED_OTA == 1)
        DeviceInfo.ota_mode.mode_flag.multi_img = 1;
#else
        DeviceInfo.ota_mode.mode_flag.multi_img = 0;
#endif
        /* meaningful only when multi_img = 1 */
        DeviceInfo.temp_bank_size =
            flash_nor_get_bank_size(FLASH_OTA_TMP) / FLASH_NOR_SECTOR_SIZE;
    }

    /*read flash map info to set image indicator*/
    get_ota_active_bank_image_info(&ota_active_bank_info);
    DeviceInfo.img_indicator = ota_active_bank_info.image_indicator;

    DeviceInfo.image_crtl_header_offset = offsetof(T_IMG_HEADER_FORMAT, ctrl_header);
    DeviceInfo.compressed_image_crtl_header_offset = 0;
    LOG_INF("<==ota_attr_read_cb: copy_img=0x%x, multi_img=0x%x, temp_bank_size=0x%x, "
            "img_indicator=0x%x",
            DeviceInfo.ota_mode.mode_flag.copy_img, DeviceInfo.ota_mode.mode_flag.multi_img,
            DeviceInfo.temp_bank_size, DeviceInfo.img_indicator);
    LOG_INF("<==ota_attr_read_cb: image_crtl_header_offset 0x%x, "
            "compressed_image_crtl_header_offset 0x%x, compress_mode %d",
            DeviceInfo.image_crtl_header_offset, DeviceInfo.compressed_image_crtl_header_offset,
            DeviceInfo.ota_mode.mode_flag.compress_mode);

    const char *value = (uint8_t *)&DeviceInfo;

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(T_DFU_DEVICE_INFO));
}

static ssize_t image_version0_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      void *buf, uint16_t len, uint16_t offset)
{
    uint8_t index = 0;
    T_IMAGE_VERSION image_ver;

    if (0 == ota_active_bank_info.image_num)
    {
        get_ota_active_bank_image_info(&ota_active_bank_info);
    }
    DFU_PRINT_INFO2("image_exist=0x%x(BIT0:OTA, BIT1:patch, ...), image_num=%d",
                    ota_active_bank_info.image_exist, ota_active_bank_info.image_num);

    for (T_IMG_ID image_id = OTA; image_id < IMAGE_MAX; image_id++)
    {
        if (ota_active_bank_info.image_exist & BIT(image_id - OTA))
        {
            get_active_bank_image_version(image_id, &image_ver);
            image_version[index] = image_ver.ver_info.version;
            index++;
            DFU_PRINT_INFO2("image:0x%x exist!, version=0x%x", image_id,
                            image_ver.ver_info.version);
        }
    }

    const char *value = (uint8_t *)image_version;
    uint16_t p_length;

    if (ota_active_bank_info.image_num > 5)
    {
        p_length = 20;
    }
    else
    {
        p_length = 4 * ota_active_bank_info.image_num;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, p_length);
}

static ssize_t image_version1_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      void *buf, uint16_t len, uint16_t offset)
{
    const char *value;
    uint16_t p_length;

    if (ota_active_bank_info.image_num > 5)
    {
        value = (uint8_t *)&image_version[5];
        if (ota_active_bank_info.image_num > 10)
        {
            p_length = 20;
        }
        else
        {
            p_length = 4 * (ota_active_bank_info.image_num - 5);
        }
        DFU_PRINT_INFO5("Image Version[5-9]: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
                        image_version[5], image_version[6], image_version[7],
                        image_version[8], image_version[9]);
    }
    else
    {
        value = NULL;
        p_length = 0;
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, p_length);
}

static ssize_t image_version2_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                      void *buf, uint16_t len, uint16_t offset)
{
    const char *value;
    uint16_t p_length;

    if (ota_active_bank_info.image_num > 10)
    {
        value = (uint8_t *)&image_version[10];
        p_length = 4 * (ota_active_bank_info.image_num - 10);
        /* ota 11 images at most */
        DFU_PRINT_INFO2("Image Version[10-11]: 0x%x, 0x%x", image_version[10],
                        image_version[11]);
    }
    else
    {
        value = NULL;
        p_length = 0;
    }

    /**
     * Notify Application
     *
     * callback_data.msg_type = SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE;
     * callback_data.msg_data.read_value_index = OTA_READ_CHAR_IMAGE_VERSION_INDEX;
     */

    /* defer processing before ota start */
    /* defer_handle = true; */

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, p_length);
}

static ssize_t protocol_type_read_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                     void *buf, uint16_t len, uint16_t offset)
{
    const char *value = (uint8_t *)&protocol_type;
    uint16_t p_length = sizeof(protocol_type);

    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, p_length);
}

#define BT_UUID_RTK_OTA_SERVICE_VAL                                                                \
    BT_UUID_128_ENCODE(0x0000d0ff, 0x3c17, 0xd293, 0x8e48, 0x14fe2e4da212)
static struct bt_uuid_128 rtk_ota_srv_uuid = BT_UUID_INIT_128(BT_UUID_RTK_OTA_SERVICE_VAL);

BT_GATT_SERVICE_DEFINE(
    rtk_ota_service, BT_GATT_PRIMARY_SERVICE(&rtk_ota_srv_uuid),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_OTA),
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, ota_write_cb, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_MAC), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, mac_address_read_cb, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_PATCH), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, patch_read_cb, NULL, NULL),
    /* */
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_APP_VERSION), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, app_version_read_cb, NULL, NULL),
    /*
     * BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_PATCH_EXTENSION),
     *          BT_GATT_CHRC_READ, BT_GATT_PERM_READ, NULL, NULL,
     *          NULL),
     * BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_TEST_MODE),
     *          BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL,
     *          ota_write_cb, NULL),
     */

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_DEVICE_INFO), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, device_info_read_cb, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_IMAGE_COUNT_TO_UPDATE),
                           BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_IMAGE_VERSION0), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, image_version0_read_cb, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_IMAGE_VERSION1), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, image_version1_read_cb, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_IMAGE_VERSION2), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, image_version2_read_cb, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(GATT_UUID_CHAR_PROTOCOL_TYPE), BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, protocol_type_read_cb, NULL, NULL));
