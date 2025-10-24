void print_all_images_version(void);
void print_flash_layout(void);

// GAP.h

#include <bt_types.h>
/** @brief  APP Return Result List */
typedef enum
{
    APP_RESULT_SUCCESS                    = (APP_SUCCESS),
    APP_RESULT_PENDING                    = (APP_ERR | APP_ERR_PENDING),
    APP_RESULT_ACCEPT                     = (APP_ERR | APP_ERR_ACCEPT),
    APP_RESULT_REJECT                     = (APP_ERR | APP_ERR_REJECT),
    APP_RESULT_NOT_RELEASE                = (APP_ERR | APP_ERR_NOT_RELEASE),

    APP_RESULT_PREP_QUEUE_FULL            = (ATT_ERR | ATT_ERR_PREP_QUEUE_FULL),
    APP_RESULT_INVALID_OFFSET             = (ATT_ERR | ATT_ERR_INVALID_OFFSET),
    APP_RESULT_INVALID_VALUE_SIZE         = (ATT_ERR | ATT_ERR_INVALID_VALUE_SIZE),
    APP_RESULT_INVALID_PDU                = (ATT_ERR | ATT_ERR_INVALID_PDU),
    APP_RESULT_ATTR_NOT_FOUND             = (ATT_ERR | ATT_ERR_ATTR_NOT_FOUND),
    APP_RESULT_ATTR_NOT_LONG              = (ATT_ERR | ATT_ERR_ATTR_NOT_LONG),
    APP_RESULT_INSUFFICIENT_RESOURCES     = (ATT_ERR | ATT_ERR_INSUFFICIENT_RESOURCES),
    APP_RESULT_VALUE_NOT_ALLOWED          = (ATT_ERR | ATT_ERR_VALUE_NOT_ALLOWED),
    APP_RESULT_APP_ERR                    = (ATT_ERR | ATT_ERR_MIN_APPLIC_CODE),
    APP_RESULT_CCCD_IMPROPERLY_CONFIGURED = (ATT_ERR | ATT_ERR_CCCD_IMPROPERLY_CONFIGURED),
    APP_RESULT_PROC_ALREADY_IN_PROGRESS   = (ATT_ERR | ATT_ERR_PROC_ALREADY_IN_PROGRESS),
} T_APP_RESULT;

// profile_server.h
typedef uint8_t T_SERVER_ID;    //!< Service ID
/** @brief Event type to inform APP*/
typedef enum
{
    SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION = 1,    /**< CCCD update event. */
    SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE = 2,              /**< client read event. */
    SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE = 3,             /**< client write event. */
} T_SERVICE_CALLBACK_TYPE;
typedef T_APP_RESULT(*P_FUN_SERVER_GENERAL_CB)(uint8_t service_id, void *p_para);


extern bool switch_to_ota_mode_pending;