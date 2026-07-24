/**
*****************************************************************************************
*     Copyright(c) 2023, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_handle.c
   * @brief     proprietary transport layer interface to application
   * @author    luke
   * @date      2023-09-28
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2018 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "board.h"
#include "trace.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "ppt_sync.h"
#include "app_section.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_sync_ctrl.h"
#include "ppt_trans_pkt_ctrl.h"
#include "ppt_trans_long_pkt_ctrl.h"
#include "ppt_trans_test_handle.h"
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
#include "ppt_trans_chann_ctrl.h"
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#include "ppt_trans_pkt_algo.h"
#endif

#if PPT_TRANS_DEBUG_PIN_ENABLE
#include "app_gpio.h"
#include "rtl_rcc.h"
#include "rtl_pinmux.h"
#include "rtl_gpio.h"
#endif

/*============================================================================*
 *                              Declares Functions
 *============================================================================*/
void ppt_trans_handle_sync_event_cb_hook(sync_event_t event);
void ppt_trans_handle_report_rate_change(T_PPT_TRANS_REPORT_RATE_REQ_TYPE dir);
void ppt_trans_handle_sample_data(T_PPT_TRANS_MOUSE_DATA mouse_data);
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
bool ppt_trans_handle_long_pkt_send(uint8_t *long_pkt_src, uint16_t length, bool req_attempt,
                                    ppt_trans_handle_long_pkt_send_compl_cb cb);
#endif
void ppt_trans_handle_notify_continuous_send(bool new_status);
bool ppt_trans_handle_get_sensor_status(void);
bool ppt_trans_handle_transport_status_get(void);
bool ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEATURES feat);
/*============================================================================*
 *                              Declares Functions
 *============================================================================*/
T_PPT_TRANS_INIT_TYPEDEF ppt_trans_handle_cfg_mgr = {0};
static const uint8_t ppt_trans_handle_m2s_payload_length[] =
{
    PPT_PKT_1K_M2S_PAYLOAD_SIZE_MAX,
    PPT_PKT_2K_M2S_PAYLOAD_SIZE_MAX,
    PPT_PKT_4K_M2S_PAYLOAD_SIZE_MAX,
    PPT_PKT_8K_M2S_PAYLOAD_SIZE_MAX
};
static const uint8_t ppt_trans_handle_s2m_payload_length[] =
{
    PPT_PKT_1K_S2M_PAYLOAD_SIZE_MAX,
    PPT_PKT_2K_S2M_PAYLOAD_SIZE_MAX,
    PPT_PKT_4K_S2M_PAYLOAD_SIZE_MAX,
    PPT_PKT_8K_S2M_PAYLOAD_SIZE_MAX
};

static void *ppt_trans_handle_notify_link_lost_timer = NULL;
static bool ppt_trans_handle_optical_sensor_enabled = false;

#if PPT_TRANS_DEBUG_PIN_ENABLE
uint8_t ppt_trans_debug_pin[] = PPT_TRANS_GPIO_DEBUG_PIN;
uint32_t ppt_trans_gpio_debug_pin[sizeof(ppt_trans_debug_pin)];
#endif
/*============================================================================*
 *                              Static Functions
 *============================================================================*/
#if PPT_TRANS_DEBUG_PIN_ENABLE

/**
 * @brief initialize gpio toggle pin for debug
 *
 */
static void ppt_trans_handle_toggle_init(void)
{
    RCC_PeriphClockCmd(APBPeriph_GPIOA, APBPeriph_GPIOA_CLOCK, (FunctionalState)ENABLE);
    for (uint8_t loop = 0; loop < sizeof(ppt_trans_debug_pin) / sizeof(ppt_trans_debug_pin[0]);
         loop++)
    {
        Pad_Config(ppt_trans_debug_pin[loop], PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE,
                   PAD_OUT_DISABLE,
                   PAD_OUT_HIGH);
        Pinmux_Config(ppt_trans_debug_pin[loop], DWGPIO);
        ppt_trans_gpio_debug_pin[loop] = GPIO_GetPin(ppt_trans_debug_pin[loop]);
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        app_gpio_struct_init(&GPIO_InitStruct);
        GPIO_InitStruct.GPIO_Pin        = ppt_trans_gpio_debug_pin[loop];
        GPIO_InitStruct.GPIO_Mode       = GPIO_MODE_OUT;
        app_gpio_init(GPIOA, &GPIO_InitStruct);
        GPIOA->GPIO_DR |= ppt_trans_gpio_debug_pin[loop];
        GPIOA->GPIO_DR &= ~ppt_trans_gpio_debug_pin[loop];
    }
}
#endif

/**
 * @brief flush pkt ctrl module handler
 *
 * @param p_timer - not used
 */
static void ppt_trans_handle_link_lost_flush_pkt(void *p_timer)
{
    if (os_timer_is_timer_active(&ppt_trans_handle_notify_link_lost_timer))
    {
        os_timer_stop(&ppt_trans_handle_notify_link_lost_timer);
    }
    ppt_trans_pkt_ctrl_init();
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
    ppt_trans_long_pkt_ctrl_init();
#endif
    ppt_trans_handle_notify_continuous_send(false);
    sync_msg_flush(SYNC_MSG_TYPE_INFINITE_RETRANS);

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    ppt_trans_sync_ctrl_init_adp_feature();
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_init();
#endif
    if (NULL != ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler)
    {
        ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler(PPT_TRANS_APPLICATION_LINK_LOST,
                                                           NULL, NULL);
    }
    APP_PRINT_INFO0("[ppt_trans_handle_link_lost_flush_pkt]");
}

/**
 * @brief  configure max send/recv payload length
 *
 */
static void ppt_trans_handle_config_payload_length(void)
{
    /** map to 1K/2K/4K/8K max payload length configuration */
    uint8_t report_rate_index = (ppt_trans_handle_cfg_mgr.report_rate / 1000) >> 1;
    report_rate_index = (report_rate_index >= sizeof(ppt_trans_handle_m2s_payload_length)) ?
                        (sizeof(ppt_trans_handle_m2s_payload_length) - 1) : report_rate_index;

    if (ppt_trans_handle_cfg_mgr.ppt_trans_role == PPT_TRANS_ROLE_MASTER)
    {
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent =
            ppt_trans_handle_m2s_payload_length[report_rate_index];
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_recv =
            ppt_trans_handle_s2m_payload_length[report_rate_index];
    }
    else
    {
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent =
            ppt_trans_handle_s2m_payload_length[report_rate_index];
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_recv =
            ppt_trans_handle_m2s_payload_length[report_rate_index];
    }
    APP_PRINT_INFO3("[ppt_trans_handle_config_payload_length] idx: %d, pkt_size_sent: %d, pkt_size_recv: %d",
                    report_rate_index, ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_sent,
                    ppt_trans_handle_cfg_mgr.ppt_max_pkt_size_recv);
}

/**
 * @brief  configure max mouse data carried in packet payload
 *
 */
static void ppt_trans_handle_config_payload_data_num(void)
{
    if (ppt_trans_handle_cfg_mgr.report_rate == PPT_REPORT_RATE_LEVEL_8K)
    {
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_mouse_data_num = PPT_PKT_8K_PAYLOAD_DATA_NUM_MAX_DV;
    }
    else
    {
        ppt_trans_handle_cfg_mgr.ppt_max_pkt_mouse_data_num = PPT_PKT_PAYLOAD_DATA_NUM_MAX_DV;
    }
}

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
/**
 * @brief  configure pos packet compose and parse algo
 *
 */
static void ppt_trans_handle_config_algo_pos_pkt(void)
{
    if (ppt_trans_handle_cfg_mgr.report_rate == PPT_REPORT_RATE_LEVEL_8K)
    {
        ppt_trans_handle_cfg_mgr.ppt_trans_algo_compose_pos_handler = ppt_trans_pkt_algo_compose_pos_pkt;
        ppt_trans_handle_cfg_mgr.ppt_trans_algo_parse_pos_handler = ppt_trans_pkt_algo_parse_pos_pkt;
    }
    else
    {
        ppt_trans_handle_cfg_mgr.ppt_trans_algo_compose_pos_handler =
            ppt_trans_pkt_algo_compose_enh_pos_pkt;
        ppt_trans_handle_cfg_mgr.ppt_trans_algo_parse_pos_handler = ppt_trans_pkt_algo_parse_enh_pos_pkt;
    }
}
#endif

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief return current status of transport layer
 *
 * @return - true if transport layer doesnot have pkt to send, otherwise false
 * @retval - bool
 */
bool ppt_trans_handle_transport_status_get(void)
{
    return ((false == ppt_trans_handle_optical_sensor_enabled)
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
            && (false == ppt_trans_long_pkt_get_is_long_pkt_transmmiting())
#endif
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
            && (true == ppt_trans_chann_ctrl_is_idle())
#endif
            && (true == ppt_trans_pkt_ctrl_get_wheel_is_send_cmpl())
           );
}

/**
 * @brief register packet receive app handler
 *
 * @param cb - callback function when any packet is received
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_pkt_receiver(ppt_trans_handle_receive_pkt_cb cb)
{
    ppt_trans_sync_ctrl_reg_receive_cb(cb);
}

/**
 * @brief register raw data receive app handler
 *
 * @param cb - callback function when any raw data is received
 *             return true if data doesn't require transport layer processing
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_raw_receiver(ppt_trans_handle_receive_raw_cb cb)
{
    ppt_trans_sync_ctrl_reg_receive_raw_cb(cb);
}

/**
 * @brief register position packet receive app handler
 *
 * @param cb - callback function when any packet is received
 *
 * @note  application layer must store the code section of the handler in RAM section by decorator RAM_FUNCTION
 */
void ppt_trans_handle_reg_pos_pkt_receiver(ppt_trans_handle_receive_pos_pkt_cb cb)
{
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_reg_receive_pos_cb(cb);
#else
    APP_PRINT_INFO0("[ppt_trans_handle_reg_pos_pkt_receiver] Feature position ctrl is not supported");
#endif
}

/**
 * @brief sync lib event hook for transport layer to prevent app scenario coupling
 *
 * @param event
 */
void ppt_trans_handle_sync_event_cb_hook(sync_event_t event)
{
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT) == false)
    {
        return;
    }

    switch (event)
    {
    case SYNC_EVENT_CONNECTED:
        {
            uint32_t ppt_interval = 125;
            sync_time_get(SYNC_TIME_PARAM_CONNECT_INTERVAL, &ppt_interval);
            ppt_trans_handle_cfg_mgr.report_rate = 1000000 / ppt_interval;
            ppt_trans_handle_config_payload_length();
            ppt_trans_handle_config_payload_data_num();
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
            ppt_trans_handle_config_algo_pos_pkt();
#endif
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
            if (ppt_trans_handle_cfg_mgr.ppt_trans_role == PPT_TRANS_ROLE_MASTER)
            {
                ppt_trans_sync_ctrl_adp_guard_timer_enable();
            }
#endif

            os_timer_stop(&ppt_trans_handle_notify_link_lost_timer);
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
            ppt_trans_long_pkt_handle_connect();
#endif
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
            if (ppt_trans_handle_cfg_mgr.ppt_trans_role == PPT_TRANS_ROLE_MASTER)
            {
                ppt_trans_chann_ctrl_handle_connect();
            }
#endif
            break;
        }
    case SYNC_EVENT_CONNECT_LOST:
        {
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
            ppt_trans_chann_ctrl_handle_disconnect();
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
            ppt_trans_pos_ctrl_handle_disconnect();
#endif
            if (false == os_timer_is_timer_active(&ppt_trans_handle_notify_link_lost_timer))
            {
                os_timer_restart(&ppt_trans_handle_notify_link_lost_timer, PPT_PKT_LINK_LOST_FLUSH_TIME);
            }
        }
        break;

    default:
        break;
    }
    return;
}

/**
 * @brief update report rate according to different mouse model
 *
 * @param dir increase or decrease report rate
 */
void ppt_trans_handle_report_rate_change(T_PPT_TRANS_REPORT_RATE_REQ_TYPE dir)
{
#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    if ((ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler == NULL) ||
        (ppt_trans_handle_cfg_mgr.ppt_trans_app_sample_rate_handler == NULL))
    {
#if PPT_TRANS_HANDLE_DBG_LOG_EN
        APP_PRINT_ERROR0("[ppt_trans_handle_report_rate_change] APP sample rate not configurable");
#endif
        return;
    }

    uint16_t report_rate, length;
    bool result = ppt_trans_handle_cfg_mgr.ppt_trans_app_req_handler(
                      PPT_TRANS_APPLICATION_RPT_RATE, (void *)&report_rate, &length);
    if (result == false)
    {
#if PPT_TRANS_HANDLE_DBG_LOG_EN
        APP_PRINT_ERROR0("[ppt_trans_handle_report_rate_change] APP sample rate get false");
#endif
        return;
    }

    switch (dir)
    {
    case PPT_TRANS_INC_REPORT_RATE_REQ:
        {
            if ((report_rate == PPT_REPORT_RATE_LEVEL_1K) && dir == PPT_TRANS_DEC_REPORT_RATE_REQ)
            {
#if PPT_TRANS_HANDLE_DBG_LOG_EN
                APP_PRINT_ERROR0("[ppt_trans_handle_report_rate_change] already reach lowest report rate");
#endif
                return;
            }
            break;
        }
    case PPT_TRANS_DEC_REPORT_RATE_REQ:
        {
            if ((report_rate == PPT_REPORT_RATE_LEVEL_8K) && dir == PPT_TRANS_INC_REPORT_RATE_REQ)
            {
#if PPT_TRANS_HANDLE_DBG_LOG_EN
                APP_PRINT_ERROR0("[ppt_trans_handle_report_rate_change] already reach highest report rate");
#endif
                return;
            }
            break;
        }
    default:
        return;
    }

    ppt_trans_handle_cfg_mgr.ppt_trans_app_sample_rate_handler(dir);
#endif
}

/**
 * @brief record sampled trajectory data
 *
 * @param mouse_data sampled SPI data
 */
void ppt_trans_handle_sample_data(T_PPT_TRANS_MOUSE_DATA mouse_data)
{
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT) == false)
    {
        return;
    }

    uint32_t s = os_lock();
#if PPT_TRANS_TEST_ENABLE
    ppt_trans_test_handle_get_testcase(&mouse_data);
#endif
    ppt_trans_pkt_ctrl_receive_data(mouse_data, false);
    // ppt_trans_handle_notify_continuous_send(true);
    os_unlock(s);
    ppt_trans_sync_ctrl_report_data();
}

#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
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
                                    ppt_trans_handle_long_pkt_send_compl_cb cb)
{
    bool status = ppt_trans_long_pkt_ctrl_reg_long_pkt_send(long_pkt_src, length, cb);
    if (status && req_attempt &&
        (false == ppt_trans_handle_get_sensor_status()))
    {
        T_PPT_TRANS_MOUSE_DATA dummy_data = {0};
        ppt_trans_pkt_ctrl_receive_data(dummy_data, true);
        ppt_trans_sync_ctrl_report_data();
    }
    return status;
}

/**
 * @brief register long packet receiving handler
 *
 * @param cb - callback function when long packet receiption completes
 *
 */
void ppt_trans_handle_long_pkt_recv(ppt_trans_handle_long_pkt_recv_compl_cb cb)
{
    ppt_trans_long_pkt_ctrl_reg_long_pkt_recv(cb);
}
#endif

/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @return bool - true -> feature enabled
 *                false -> feature disabled
 */
bool ppt_trans_handle_get_feature_status(T_PPT_TRANS_FEATURES feat)
{
    return ppt_trans_feat_ctrl_get_feature_status(feat);
}

/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @param  enable - true -> feature enabled
 *                  false -> feature disabled
 */
void ppt_trans_handle_set_feature_status(T_PPT_TRANS_FEATURES feat, bool enable)
{
    ppt_trans_feat_ctrl_set_feature_status(feat, enable);
    switch (feat)
    {
    case T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT:
        {
            if (enable)
            {
                ppt_trans_sync_ctrl_reset_receive_cb();
            }
            else
            {
                void *dummy_param = NULL;
                ppt_trans_handle_link_lost_flush_pkt(dummy_param);
            }
            break;
        }

    default:
        break;
    }
}

/**
 * @brief initialize ppt transport layer configuration
 *
 * @param init_typedef - config structure
 */
void ppt_trans_handle_struct_init(T_PPT_TRANS_INIT_TYPEDEF *init_typedef)
{
    init_typedef->ppt_trans_role = PPT_TRANS_ROLE_MASTER;
    init_typedef->ppt_trans_app_rx_handler = NULL;
    init_typedef->ppt_trans_app_rx_raw_handler = NULL;
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    init_typedef->ppt_trans_app_pos_rx_handler = NULL;
    init_typedef->ppt_trans_algo_compose_pos_handler = NULL;
    init_typedef->ppt_trans_algo_parse_pos_handler = NULL;
#endif
    init_typedef->ppt_trans_app_feature_mask = BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_NUM) - 1;
    init_typedef->ppt_trans_app_sample_rate_handler = NULL;
    init_typedef->ppt_trans_app_req_handler = NULL;
    init_typedef->report_rate = PPT_REPORT_RATE_LEVEL_8K;
    init_typedef->ppt_max_pkt_size_sent = PPT_PKT_8K_M2S_PAYLOAD_SIZE_MAX;
    init_typedef->ppt_max_pkt_size_recv = PPT_PKT_8K_S2M_PAYLOAD_SIZE_MAX;
    init_typedef->ppt_max_pkt_mouse_data_num = PPT_PKT_8K_PAYLOAD_DATA_NUM_MAX_DV;
    return;
}

/**
 * @brief notify optical sensor status working or not
 *
 */
void ppt_trans_handle_notify_continuous_send(bool new_status)
{
    if (new_status != ppt_trans_handle_optical_sensor_enabled)
    {
#if PPT_TRANS_HANDLE_DBG_LOG_EN
        APP_PRINT_INFO2("[ppt_trans_handle_notify_continuous_send] %d -> %d",
                        ppt_trans_handle_optical_sensor_enabled, new_status);
#endif
        uint32_t s = os_lock();
        ppt_trans_handle_optical_sensor_enabled = new_status;
        os_unlock(s);

        if ((false == ppt_trans_handle_optical_sensor_enabled) &&
            (false == ppt_trans_handle_transport_status_get()))
        {
            T_PPT_TRANS_MOUSE_DATA dummy_data = {0};
            ppt_trans_pkt_ctrl_receive_data(dummy_data, true);
            ppt_trans_sync_ctrl_report_data();
        }
    }
}

/**
 * @brief get optical sensor working status
 * @retval true -> sensor sending continuously
 */
bool ppt_trans_handle_get_sensor_status(void)
{
    return ppt_trans_handle_optical_sensor_enabled;
}

/**
 * @brief configure pair id from application layer
 *
 * @param  pair_id - pair id to be passed to be set
 * @return bool - true -> pair id successfully set
 *              - false -> pair id had failed to set
 */
bool ppt_trans_handle_set_pair_id(uint32_t pair_id)
{
#if PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_PAIRING_ID))
    {
        return ppt_trans_sync_ctrl_set_pair_id(pair_id);
    }
    APP_PRINT_INFO0("[ppt_trans_handle_get_pair_id] pairing ID feature disabled");
    return false;
#else
    APP_PRINT_INFO0("[ppt_trans_handle_set_pair_id] Configure pairing ID is not supported");
    return false;
#endif
}

bool ppt_trans_handle_get_pair_id(uint32_t *pair_id)
{
#if PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID
    if (ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEAT_CTRL_FEATURE_PAIRING_ID))
    {
        return ppt_trans_sync_ctrl_get_pair_id(pair_id);
    }
    APP_PRINT_INFO0("[ppt_trans_handle_get_pair_id] pairing ID feature disabled");
    return false;
#else
    APP_PRINT_INFO0("[ppt_trans_handle_get_pair_id] Obtain pairing ID is not supported");
    return false;
#endif
}


/**
 * @brief initialization of transport layer module
 *
 * @param role - transport layer initialized as master or slave
 */
void ppt_trans_handle_init(T_PPT_TRANS_INIT_TYPEDEF *init_typedef)
{
    memcpy(&ppt_trans_handle_cfg_mgr, init_typedef, sizeof(T_PPT_TRANS_INIT_TYPEDEF));
    ppt_trans_feat_ctrl_init(init_typedef->ppt_trans_app_feature_mask);
    ppt_trans_sync_ctrl_init();
    ppt_trans_handle_config_payload_length();
    ppt_trans_handle_config_payload_data_num();

    if (ppt_trans_handle_cfg_mgr.ppt_trans_role == PPT_TRANS_ROLE_MASTER)
    {
#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
        ppt_trans_chann_ctrl_master_init();
#endif
        ppt_trans_handle_reg_raw_receiver(ppt_trans_handle_cfg_mgr.ppt_trans_app_rx_raw_handler);
    }
    else
    {
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
        ppt_trans_handle_reg_pos_pkt_receiver(ppt_trans_handle_cfg_mgr.ppt_trans_app_pos_rx_handler);
#else
        ppt_trans_handle_reg_pkt_receiver(ppt_trans_handle_cfg_mgr.ppt_trans_app_rx_handler);
#endif
        ppt_trans_handle_reg_raw_receiver(ppt_trans_handle_cfg_mgr.ppt_trans_app_rx_raw_handler);

#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
        ppt_trans_chann_ctrl_slave_init();
#endif
    }

    ppt_trans_handle_optical_sensor_enabled = false;

    if (false == os_timer_create(&ppt_trans_handle_notify_link_lost_timer,
                                 "ppt_trans_handle_notify_link_lost_timer", 0, PPT_PKT_LINK_LOST_FLUSH_TIME,
                                 false, ppt_trans_handle_link_lost_flush_pkt))
    {
        APP_PRINT_ERROR0("[ppt_trans_handle_init] flush timer creation failed");
    }

    ppt_trans_pkt_ctrl_init();
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT_WITHIN_4K
    ppt_trans_long_pkt_ctrl_init();
#endif

#if PPT_TRANS_TEST_ENABLE
    ppt_trans_test_handle_init();
#endif

#if PPT_TRANS_DEBUG_PIN_ENABLE
    ppt_trans_handle_toggle_init();
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    ppt_trans_pos_ctrl_init();
    ppt_trans_handle_config_algo_pos_pkt();
#endif
    APP_PRINT_INFO1("[ppt_trans_handle_init] PPT Transport layer ver "PPT_TRANSPORT_VERSION", role: %d",
                    ppt_trans_handle_cfg_mgr.ppt_trans_role);
}
