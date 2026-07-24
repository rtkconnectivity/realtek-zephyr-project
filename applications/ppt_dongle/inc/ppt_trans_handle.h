/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_handle.h
   * @brief     proprietary transport layer interface to application
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */
#ifndef __PPT_TRANS_HANDLE_H__
#define __PPT_TRANS_HANDLE_H__

#ifdef __cplusplus
extern "C"  {
#endif      /* __cplusplus */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "rtl876x.h"
#include "ppt_sync.h"

#define PPT_TRANSPORT_VERSION                       "v1.2.2"
/*============================================================================*
 *                    Transport Layer Feature Control Macros
 *============================================================================*/
#define PPT_TRANS_FEATURE_TRANSPORT_EN              1       /** Enable transport layer send/receive or not */
#define PPT_TRANS_FEATURE_PKT_RETRANS_EN            1       /** Enable mouse packet retransmission */
#define PPT_TRANS_FEATURE_ADP_RPT_RATE_EN           0       /** Enable adaptive report rate change */

#if FEATURE_SUPPORT_PROPRIETARY_HOPPING
#define PPT_TRANS_FEATURE_CONFIG_CHAN               1       /** Enable configure channel by transport layer */
/*Channel Sweep Enable Start Example*/
#define PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL      0       /** Enable channel statistic control method */
/*Channel Sweep Enable End Example*/
/*Channel Traverse Enable Start Example*/
#define PPT_TRANS_FEATURE_SUPPORT_CHANNEL_TRAVERSE  1       /** Enable channel travese control method */
/*Channel Traverse Enable End Example*/
#define PPT_TRANS_FEATURE_SUPPORT_16BITS_MOV        1       /** Enable 16btis movement data */
#define PPT_TRANS_FEATURE_SUPPORT_POS_CTRL          1       /** Enable position control feature*/
#define PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID        0       /** Enable configure pairing ID by application */
#else
#define PPT_TRANS_FEATURE_CONFIG_CHAN               0       /** Enable configure channel by transport layer */
#define PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL      0       /** Enable syncing cursor offset */
#define PPT_TRANS_FEATURE_SUPPORT_CHANNEL_TRAVERSE  0       /** Enable channel travese control method */
#define PPT_TRANS_FEATURE_SUPPORT_16BITS_MOV        1       /** Enable 16btis movement data */
#define PPT_TRANS_FEATURE_SUPPORT_POS_CTRL          1       /** Enable position control feature*/
#define PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID        0       /** Enable configure pairing ID by application */
#endif

/*Channel Limit Start Example*/
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL && PPT_TRANS_FEATURE_SUPPORT_CHANNEL_TRAVERSE
#error "Cannot enable both feature simutaniously"
#endif
/*Channel Limit End Example*/
/*============================================================================*
 *                              Sync Control Macros
 *============================================================================*/
#define PPT_REPORT_RATE_LEVEL_8K                    8000
#define PPT_REPORT_RATE_LEVEL_4K                    4000
#define PPT_REPORT_RATE_LEVEL_2K                    2000
#define PPT_REPORT_RATE_LEVEL_1K                    1000

/** if no external data from application within this time of period,
 *  Transport layer will change to idle state and check if all packet sending completed (ex. channel sweep, long packet) */
#define PPT_TRANS_SYNC_CTRL_REPORT_IDLE_ITVL        50 // ms

#if PPT_TRANS_FEATURE_CONFIG_CHAN
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_TRAVERSE
#define PPT_TRANS_SYNC_CHANS                        2450, 2412, 2437, 2423, 2479, 2428, 2462, 2404, 2442
#else
#define PPT_TRANS_SYNC_CHANS                        2450, 2404, 2477, 2412, 2467, 2437, 2423, 2457, 2479, 2428, 2462, 2425, 2447, 2405, 2442
#endif
#endif

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
/** history packets checked, non changable */
#define PPT_WATCH_HIST_PKT_CNT                  96
/** criterion to increase report rate */
#define PPT_INC_RPT_RATE_CNT                    95
/** criterion to decrease report rate */
#define PPT_DEC_RPT_RATE_CNT                    50
/** disable report rate change for this period, unit ms */
#define PPT_ADP_RPT_GUARD_TIME                  6000
#endif

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
/** size of the history data list to store failed mouse data */
#define PPT_FAIL_PKT_LIST_SIZE                  16
#endif

/*============================================================================*
 *                              Packet Control Macros
 *============================================================================*/
/** Size of history packets stored */
#define PPT_PKT_HIST_PKT_SIZE                   16
/** Size of missed wheel data stored */
#define PPT_PKT_HIST_WHEEL_ARR_SIZE             64
/** Flush pkt ctrl buffer if link lost and didn't reconnect afterwards, unit: ms */
#define PPT_PKT_LINK_LOST_FLUSH_TIME            500

/** Size of each packet payload from master to slave, unit: bytes */
#define PPT_PKT_8K_M2S_PAYLOAD_SIZE_MAX         7
#define PPT_PKT_4K_M2S_PAYLOAD_SIZE_MAX         12
#define PPT_PKT_2K_M2S_PAYLOAD_SIZE_MAX         20
#define PPT_PKT_1K_M2S_PAYLOAD_SIZE_MAX         30

/** Size of each packet payload from slave to master, unit: bytes */
#define PPT_PKT_8K_S2M_PAYLOAD_SIZE_MAX         3
#define PPT_PKT_4K_S2M_PAYLOAD_SIZE_MAX         12
#define PPT_PKT_2K_S2M_PAYLOAD_SIZE_MAX         20
#define PPT_PKT_1K_S2M_PAYLOAD_SIZE_MAX         30

/*============================================================================*
 *                              Packet Algorithm Macros
 *============================================================================*/
/** Size of each field contained in packet header, unit: bits */
#define PPT_PKT_HDR_SIZE_SEQ_EN                 1
#define PPT_PKT_HDR_SIZE_BUTTON_EN              1
#define PPT_PKT_HDR_SIZE_WHEEL_EN               1
#define PPT_PKT_HDR_SIZE_MOTION_EN              3
#define PPT_PKT_HDR_SIZE_LONG_PKT_EN            1
#define PPT_PKT_HDR_SIZE_PKT_TYPE               1
#define PPT_PKT_HDR_SIZE_RSVD                   1

/** Size of each field contained in packet payload, unit: bits */
#define PPT_PKT_PAYLOAD_SIZE_HEADER             8
#define PPT_PKT_PAYLOAD_SIZE_SEQ                6
#define PPT_PKT_PAYLOAD_SIZE_BUTTON             7
#define PPT_PKT_PAYLOAD_SIZE_WHEEL              3

/** MAX numbers of each mouse data carried in a payload */
#define PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV         2
#define PPT_PKT_8K_PAYLOAD_DATA_NUM_MAX_DV      2

/** Numbers of all possible sequence number */
#define PPT_PKT_PAYLOAD_SEQ_NUM_SIZE            BIT(PPT_PKT_PAYLOAD_SIZE_SEQ)

/*============================================================================*
 *                              Transport layer configuration Macros
 *============================================================================*/
#define PPT_PKT_PAYLOAD_DATA_NUM_MAX            ppt_trans_handle_cfg_mgr.ppt_max_pkt_mouse_data_num

/*============================================================================*
 *                              Debug Macros
 *============================================================================*/
/** DBG LOG control macro */
#define PPT_TRANS_DEBUG_LOG_ENABLE              0
#define PPT_TRANS_HANDLE_DBG_LOG_EN             (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_PKT_ALGO_DBG_LOG_EN           (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_PKT_CTRL_DBG_LOG_EN           (1 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_LONG_PKT_CTRL_DBG_LOG_EN      (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_SYNC_CTRL_DBG_LOG_EN          (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_TEST_HANDLE_DBG_LOG_EN        (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_TEST_STUB_SYNC_DBG_LOG_EN     (1 && PPT_TRANS_DEBUG_LOG_ENABLE)
#define PPT_TRANS_CHANN_CTRL_DBG_LOG            (0 && PPT_TRANS_DEBUG_LOG_ENABLE)
/** dump packet send status as log */
#define PPT_TRANS_SYNC_CTRL_REPORT_STAT            0
/** interval between each packet send status log */
#define PPT_TRANS_SYNC_CTRL_REPORT_STAT_ITVL       1000

#define PPT_TRANS_DEBUG_PIN_ENABLE                 0

#define PPT_TRANS_GPIO_DEBUG_PIN                  {P3_0, P3_1, P1_0, P1_1}
#define TIME_DEBUG_CHAN_INVALID                 -1
#define TIME_DEBUG_CHAN_TX_SEND                 -1
#define TIME_DEBUG_CHAN_TX_COMPOSE_PKT          -1
#define TIME_DEBUG_CHAN_TX_COMPENSATE           -1
#define TIME_DEBUG_CHAN_TX_CB                   -1
#define TIME_DEBUG_CHAN_TX_SAMPLE               -1
#define TIME_DEBUG_CHAN_TX_FILL                 -1
#define TIME_DEBUG_CHAN_RX_RECV                 -1
#define TIME_DEBUG_CHAN_RX_PARSE                -1
#define TIME_DEBUG_SENSOR_INT                   -1
#define TIME_DEBUG_PPT_TRANSPORT_USB_MSG_SEND   -1

#if PPT_TRANS_DEBUG_PIN_ENABLE

extern uint8_t ppt_trans_debug_pin[];
extern uint32_t ppt_trans_gpio_debug_pin[];
#define PPT_TRANS_GPIO_LEVEL_HIGH(ch)   do \
    { \
        if(ch != TIME_DEBUG_CHAN_INVALID) \
        {\
            GPIOA->GPIO_DR |= ppt_trans_gpio_debug_pin[ch]; \
        }\
    }while(0)
#define PPT_TRANS_GPIO_LEVEL_LOW(ch)    do \
    { \
        if(ch != TIME_DEBUG_CHAN_INVALID) \
        {\
            GPIOA->GPIO_DR &= ~ppt_trans_gpio_debug_pin[ch]; \
        }\
    }while(0)
#define PPT_TRANS_GPIO_EDGE_UP(ch)      do \
    { \
        if(ch != TIME_DEBUG_CHAN_INVALID) \
        {\
            GPIOA->GPIO_DR &= ~ppt_trans_gpio_debug_pin[ch]; \
            GPIOA->GPIO_DR |= ppt_trans_gpio_debug_pin[ch]; \
        }\
    }while(0)
#define PPT_TRANS_GPIO_EDGE_DOWN(ch)      do \
    { \
        if(ch != TIME_DEBUG_CHAN_INVALID) \
        {\
            GPIOA->GPIO_DR |= ppt_trans_gpio_debug_pin[ch]; \
            GPIOA->GPIO_DR &= ~ppt_trans_gpio_debug_pin[ch]; \
        }\
    }while(0)
#else
#define PPT_TRANS_GPIO_LEVEL_HIGH
#define PPT_TRANS_GPIO_LEVEL_LOW
#define PPT_TRANS_GPIO_EDGE_UP
#define PPT_TRANS_GPIO_EDGE_DOWN
#endif
/*============================================================================*
 *                              Typedefs
 *============================================================================*/
/**
 * @brief PPT transport layer module role definition
 *
 */
typedef enum
{
    PPT_TRANS_ROLE_MASTER            = 0,
    PPT_TRANS_ROLE_SLAVE,
} T_PPT_TRANS_ROLES;


typedef enum
{
    PPT_TRANS_APPLICATION_RPT_RATE   = 0,
    PPT_TRANS_APPLICATION_LINK_LOST
} T_PPT_TRANS_APPLICATION_DATA;

/**
 * @brief PPT transport layer ENABLE/DISABLE definition
 *
 */
typedef enum
{
    PPT_TRANS_FIELD_DISABLE          = 0,
    PPT_TRANS_FIELD_ENABLE           = 1
} T_PPT_TRANS_FUNCTIONAL_STATE;

/**
 * @brief PPT transport layer Packet type definition
 *
 */
typedef enum
{
    PPT_TRANS_PKT_TYPE_OFFSET        = 0,
    PPT_TRANS_PKT_TYPE_COMPENSATE    = 1
} T_PPT_TRANS_PACKET_TYPE;

/**
 * @brief  PPT transport layer report rate change request.
 */
typedef enum
{
    PPT_TRANS_INC_REPORT_RATE_REQ        = 0,
    PPT_TRANS_DEC_REPORT_RATE_REQ        = 1,
    PPT_TRANS_FORCE_INC_REPORT_RATE_REQ  = 2,
} T_PPT_TRANS_REPORT_RATE_REQ_TYPE;

/**
 * @brief PPT transport layer module mouse data definition
 *
 */
typedef struct
{
    uint8_t                 wheel_direction;
    uint8_t                 button;
    int16_t                 optical_x;
    int16_t                 optical_y;
} T_PPT_TRANS_MOUSE_DATA;

/**
 * @brief PPT transport layer motion data size definition
 *
 */
typedef enum
{
    PPT_TRANS_MOTION_SIZE_0BITS = 0,
    PPT_TRANS_MOTION_SIZE_2BITS,
    PPT_TRANS_MOTION_SIZE_4BITS,
    PPT_TRANS_MOTION_SIZE_8BITS,
    PPT_TRANS_MOTION_SIZE_16BITS
} T_PPT_TRANS_MOTION_DATA_SIZE;

/**
 * @brief PPT transport layer wheel direction definition
 *
 */
typedef enum
{
    PPT_TRANS_WHEEL_RELEASE_DEF  = 0x0,
    PPT_TRANS_WHEEL_V_UP_DEF,
    PPT_TRANS_WHEEL_V_DOWN_DEF,
    PPT_TRANS_WHEEL_H_UP_DEF,
    PPT_TRANS_WHEEL_H_DOWN_DEF,
} T_PPT_TRANS_WHEEL_DIRECTION;

/**
 * @brief PPT transport layer feature list
 *
 */
typedef enum
{
    T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT      = 0,    /** enable transport layer or not */
    T_PPT_TRANS_FEAT_CTRL_FEATURE_PKT_RETRANS,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_ADP_RATE,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_CONFIG_CHAN,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_CHAN_CTRL,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_POS_CTRL,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV,
    T_PPT_TRANS_FEAT_CTRL_FEATURE_PAIRING_ID,

    T_PPT_TRANS_FEAT_CTRL_DEBUG_REPORT_STAT,

    T_PPT_TRANS_FEAT_CTRL_FEATURE_NUM
} T_PPT_TRANS_FEATURES;


/**
 * @brief PPT transport layer packet header definition
 *
 */
typedef union _T_PPT_TRANS_PKT_HEADER
{
    struct
    {
uint8_t long_pkt_en : PPT_PKT_HDR_SIZE_LONG_PKT_EN;
uint8_t motion_size : PPT_PKT_HDR_SIZE_MOTION_EN;
uint8_t wheel_en    : PPT_PKT_HDR_SIZE_WHEEL_EN;
uint8_t button_en   : PPT_PKT_HDR_SIZE_BUTTON_EN;
uint8_t pkt_type    : PPT_PKT_HDR_SIZE_PKT_TYPE;
uint8_t seq_en      : PPT_PKT_HDR_SIZE_SEQ_EN;
    };
    uint8_t d8;
} T_PPT_TRANS_PKT_HEADER;

typedef struct
{
    uint8_t                 wheel_direction;
    uint8_t                 button;
    int32_t                 optical_x;
    int32_t                 optical_y;
} T_PPT_MOUSE_ABS_POS_DATA;

typedef void (*ppt_trans_handle_receive_pos_pkt_cb)(T_PPT_MOUSE_ABS_POS_DATA *data, uint8_t len,
                                                    uint8_t seq_num);
typedef void (*ppt_trans_handle_algo_compose_pos)(T_PPT_TRANS_MOUSE_DATA *data_src,
                                                  uint8_t length_src,
                                                  uint8_t *data_pos_in, uint8_t len_pos,
                                                  uint8_t *data_out, uint8_t *length_out, uint8_t seq_num);
typedef void (*ppt_trans_handle_algo_parse_pos)(uint8_t *data_src, uint16_t length_src,
                                                uint8_t *pos_out, uint16_t length_pos,
                                                T_PPT_TRANS_MOUSE_DATA *but_out, uint8_t *length_out,
                                                uint8_t *seq_num);

typedef void (*ppt_trans_handle_long_pkt_send_compl_cb)(bool result, sync_send_info_t info);
typedef void (*ppt_trans_handle_long_pkt_recv_compl_cb)(bool result, uint8_t *pkt_data,
                                                        uint16_t len, sync_receive_info_t info);
typedef void (*ppt_trans_handle_receive_pkt_cb)(T_PPT_TRANS_MOUSE_DATA *data, uint8_t len,
                                                uint8_t seq_num);
typedef bool (*ppt_trans_handle_receive_raw_cb)(uint8_t *p_data, uint16_t len,
                                                sync_receive_info_t *info);
typedef bool (*ppt_trans_handle_get_application_data)(T_PPT_TRANS_APPLICATION_DATA request,
                                                      void *data, uint16_t *len);

typedef void (*ppt_trans_handle_set_sample_rate)(T_PPT_TRANS_REPORT_RATE_REQ_TYPE dir);

typedef struct _T_PPT_TRANS_INIT_TYPEDEF
{
    T_PPT_TRANS_ROLES                           ppt_trans_role;
    ppt_trans_handle_get_application_data       ppt_trans_app_req_handler;
    ppt_trans_handle_set_sample_rate            ppt_trans_app_sample_rate_handler;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_handle_receive_pos_pkt_cb         ppt_trans_app_pos_rx_handler;
    ppt_trans_handle_algo_compose_pos           ppt_trans_algo_compose_pos_handler;
    ppt_trans_handle_algo_parse_pos             ppt_trans_algo_parse_pos_handler;
#endif
    ppt_trans_handle_receive_pkt_cb             ppt_trans_app_rx_handler;
    ppt_trans_handle_receive_raw_cb             ppt_trans_app_rx_raw_handler;
    uint32_t                                    report_rate;
    uint32_t                                    ppt_trans_app_feature_mask;
    uint8_t                                     ppt_max_pkt_size_sent;
    uint8_t                                     ppt_max_pkt_size_recv;
    uint8_t                                     ppt_max_pkt_mouse_data_num;
} T_PPT_TRANS_INIT_TYPEDEF;

extern T_PPT_TRANS_INIT_TYPEDEF ppt_trans_handle_cfg_mgr;

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief initialization of transport layer module
 *
 */
void ppt_trans_handle_init(T_PPT_TRANS_INIT_TYPEDEF *init_typedef);

/**
 * @brief initialize ppt transport layer configuration
 *
 * @param init_typedef - config structure
 */
void ppt_trans_handle_struct_init(T_PPT_TRANS_INIT_TYPEDEF *init_typedef);

/**
 * @brief
 *
 * @param event
 */
void ppt_trans_handle_sync_event_cb_hook(sync_event_t event);

/**
 * @brief update report rate according to different mouse model
 *
 * @param dir increase or decrease report rate
 */
void ppt_trans_handle_report_rate_change(T_PPT_TRANS_REPORT_RATE_REQ_TYPE dir);

/**
 * @brief record sampled trajectory data
 *
 * @param mouse_data sampled SPI data
 */
void ppt_trans_handle_sample_data(T_PPT_TRANS_MOUSE_DATA mouse_data);

/**
 * @brief return current status of transport layer
 *
 * @return - true if transport layer doesnot have pkt to send, otherwise false
 * @retval - bool
 */
bool ppt_trans_handle_transport_status_get(void);

/**
 * @brief handle long packet sending
 *
 * @param long_pkt_src - pointer to long packet
 * @param length - packet length
 * @param req_attempt - attempt to send long packet right after reg sending.
 * @param cb - callback function when long packet transmission completes
 *
 * @note  application layer must keep the buffer alive while processing it
 */
bool ppt_trans_handle_long_pkt_send(uint8_t *long_pkt_src, uint16_t length, bool req_attempt,
                                    ppt_trans_handle_long_pkt_send_compl_cb cb);

/**
 * @brief register long packet receiving handler
 *
 * @param cb - callback function when long packet receiption completes
 *
 */
void ppt_trans_handle_long_pkt_recv(ppt_trans_handle_long_pkt_recv_compl_cb cb);

/**
 * @brief register packet receive app handler
 *
 * @param cb - callback function when any packet is received
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_pkt_receiver(ppt_trans_handle_receive_pkt_cb cb);

/**
 * @brief register raw data receive app handler
 *
 * @param cb - callback function when any raw data is received
 *             return true if data doesn't require transport layer processing
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_raw_receiver(ppt_trans_handle_receive_raw_cb cb);

/**
 * @brief register position packet receive app handler
 *
 * @param cb - callback function when any packet is received
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_pos_pkt_receiver(ppt_trans_handle_receive_pos_pkt_cb cb);

/**
 * @brief notify that application will start/stop continuous sending data
 *
 */
void ppt_trans_handle_notify_continuous_send(bool new_status);

/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @return bool - true -> feature enabled
 *                false -> feature disabled
 */
bool ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEATURES feat);


/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @param  enable - true -> feature enabled
 *                  false -> feature disabled
 */
void ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEATURES feat, bool enable);

/**
 * @brief get optical sensor working status
 *
 */
bool ppt_trans_handle_get_sensor_status(void);

/**
 * @brief get pair id used for sync layer
 *
 * @param  pair_id - pair id to be passed to be set
 * @return bool - true -> pair id successfully get
 *              - false -> pair id had failed to get
 */
bool ppt_trans_handle_get_pair_id(uint32_t *pair_id);


/**
 * @brief configure pair id from application layer
 *
 * @param  pair_id - pair id to be passed to be set
 * @return bool - true -> pair id successfully set
 *              - false -> pair id had failed to set
 */
bool ppt_trans_handle_set_pair_id(uint32_t pair_id);

#ifdef __cplusplus
}
#endif      /* __cplusplus */

#endif // __PPT_TRANS_HANDLE_H__