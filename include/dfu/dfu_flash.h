/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
  * @file dfu_flash.h
  * @brief
  * @details
  * @author
  * @date
  * @version
  ***************************************************************************************
    * @attention
  * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
  ***************************************************************************************
  */

/*============================================================================*
 *               Define to prevent recursive inclusion
 *============================================================================*/
#ifndef DFU_FLASH_H
#define DFU_FLASH_H

/*============================================================================*
 *                        Header Files
 *============================================================================*/
#include <stdint.h>
#include <stdbool.h>
#include "patch_header_check.h"
#include "dfu_api.h"

/** @defgroup DFU_API DFU
  * @brief API sets for device firmware update implementation
  * @{
  */
/*============================================================================*
  *                                   Types
 *============================================================================*/
/** @defgroup DFU_API_Exported_Types DFU Exported Types
    * @brief
    * @{
    */

typedef enum
{
    DFU_START = 0,
    DFU_DOING,
    DFU_END,
} T_DFU_STATUS;

typedef union
{
    T_IMG_CTRL_HEADER_FORMAT *p_header;
    uint32_t dfu_length;
    bool image_check_result;
} T_DFU_DATA;

typedef void (*P_FUNC_DFU_STATUS_CB)(T_DFU_STATUS status, T_DFU_DATA data);

/** End of DFU_API_Exported_Types
  * @}
  */

/*============================================================================*
  *                                Variables
  *============================================================================*/
/** @defgroup DFU_API_Exported_Functions DFU Exported Variables
    * @{
    */
extern bool dfu_compressed_mode;
/** End of DFU_API_Exported_Variables
  * @}
  */

/*============================================================================*
  *                                Functions
  *============================================================================*/
/** @defgroup DFU_API_Exported_Functions DFU Exported Functions
    * @{
    */
/**
 * @brief  Check ota mode flag, if image need update.
 * @return Result: true: image need update, false: image don't need update.
 */
bool dfu_check_ota_mode_flag(void);

/**
 * @brief  Set AON register value indicating whether in OTA mode or not.
 * @param  enable  OTA mode flag
 */
void dfu_set_ota_mode_flag(bool enable);

/**
 * @brief  Switch to the OTA mode, if supported, normal OTA app needs to call it.
 * @return None
 */
void dfu_switch_to_ota_mode(void);

/**
 * @brief  OTA procedure performs WDG system reset.
 * @param  is_active_fw true means DFU success, otherwise fail.
 */
void dfu_fw_reboot(bool is_active_fw);

/**
 * @brief  Set specified image not_ready bit.
 * @param  p_header      pointer to p_header
 */
void dfu_set_ready(T_IMG_HEADER_FORMAT *p_header);

/**
 * @brief  Set specified image not_obsolete bit.
 * @param  p_header      pointer to p_header
 */
void dfu_set_obsolete(T_IMG_HEADER_FORMAT *p_header);

/**
 * @brief  Get encrypted Bluetooth transmission setting.
 * @return true if encrypted, false otherwise
 */
bool dfu_get_enc_setting(void);

/**
 * @brief  Decrypt an OTA image using AES.
 * @param  input      pointer to the encrypted input data
 * @param  output     pointer to the buffer where decrypted data will be written
 * @param  length     length of the encrypted input data
 */
void dfu_hw_aes_decrypt_image(uint8_t *input, uint8_t *output, uint32_t length);

/**
 * @brief  Encrypt DFU handshake data using AES.
 * @param  data[16]  an array of 16 bytes containing the handshake data to be encrypted
 */
void dfu_encrypt_handshake_data(uint8_t data[16]);

/**
 * @brief  Get image address of specified image located in OTA active bank.
 * @param  image_id     specify the image located in OTA active bank
 * @return image address of specified image located in OTA active bank
 */
uint32_t dfu_get_active_bank_image_addr_by_img_id(T_IMG_ID image_id);

/**
 * @brief  Get size of specified image located in OTA temp bank.
 * @param  image_id     specify the image
 * @return size of specified image located in OTA temp bank
 */
uint32_t dfu_get_temp_ota_bank_addr_by_img_id(T_IMG_ID image_id);

/**
 * @brief  Get size of specified image located in OTA temp bank.
 * @param  image_id     specifies the image
 * @return size of specified image located in OTA temp bank
 */
uint32_t dfu_get_temp_ota_bank_size_by_img_id(T_IMG_ID image_id);

/**
 * @brief  Report specified target IC type.
 * @param  image_id     image_id to identify image
 * @param  p_ic_type    pointer to the IC type
 * @return 0 if report IC type info successfully, error line number otherwise
*/
uint32_t dfu_report_target_ic_type(uint16_t image_id, uint8_t *p_ic_type);

/**
 * @brief  Report specified FW info and current OTA offset.
 * @param  image_id                image_id to identify FW
 * @param  p_origin_fw_version     pointer to the current FW version
 * @param  p_offset                pointer to the current file offset
 * @return 0 if report FW info successfully, error line number otherwise
*/
uint32_t dfu_report_target_fw_info(uint16_t image_id, uint32_t *p_origin_fw_version,
                                   uint32_t *p_offset);

/**
 * @brief  Check the CRC of a buffer.
 * @param  buf                pointer to the buffer containing the data to be checked
 * @param  length             length of the buffer in bytes
 * @param  crc_val            expected CRC value for the buffer
 * @return 0 if buffer checksum calculated successfully, error line number otherwise
 */
uint32_t dfu_check_buf_crc(uint8_t *buf, uint32_t length, uint16_t crc_val);


/**
 * @brief  Unlock flash when erasing or writing flash.
 * @return true: success ; false: fail
 */
bool unlock_flash_bp_all(void);

/**
 * @brief  Lock flash after erasing or writing flash.
 */
void lock_flash_bp(void);

/**
 * @brief  Erase a sector of the flash, will retry three times at most.
 * @param  image_id          image_id to identify FW
 * @param  offset            offset of the image
 * @return 0 if erases successfully, error line number otherwise
 */
uint32_t dfu_flash_erase_sector_with_retry(uint16_t image_id, uint32_t offset);

/**
 * @brief  Erase a sector of the flash.
 * @param  addr            addr of the flash
 * @return true: success ; false: fail
 */
bool dfu_flash_erase_sector(uint32_t addr);

/**
 * @brief  Write specified image data with specified length to flash.
 * @param  image_id           image_id to identify FW
 * @param  offset             offset of the image
 * @param  total_offset       total offset in temp bank
 * @param  length             length of data
 * @param  p_void             pointer to data
 * @return 0 if write FW image successfully, error line number otherwise
 */
uint32_t dfu_update(uint16_t image_id, uint32_t offset, uint32_t total_offset, uint32_t length,
                    uint32_t *p_void);


/**
 * @brief   Calculate checksum of the image.
 * @param   image_id   image_id to identify image
 * @param   offset     image offset in temp bank
 * @return  true: success ; false: fail
 */
bool dfu_check_checksum(uint16_t image_id, uint32_t offset);


/**
 * @brief  Set specified image not_ready bit.
 * @param  p_header         pointer to p_header
 * @param  compressed_mode  is compressed mode or not
 */
void dfu_set_image_ready(T_IMG_HEADER_FORMAT *p_header, bool compressed_mode);


/**
 * @brief   Copy APP Data from active bank to updating bank.
 * @param   image_id    image_id to identify the image
 * @param   dlAddress   address the image is copied to
 * @param   dlSize      copy size
 * @return  true if the image is copied successfully, false otherwise
 */
bool dfu_copy_img(uint16_t image_id, uint32_t dlAddress, uint32_t dlSize);


/**
 * @brief   Erase a sector of the flash.
 * @param   addr    flash address to erase
 * @return  true if erase successfully, false otherwise
 */
bool dfu_flash_erase_sector(uint32_t addr);

/** End of DFU_API_Exported_Functions
  * @}
  */

/** End of DFU_API
  * @}
  */


#endif //DFU_FLASH_H
