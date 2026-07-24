/**
*****************************************************************************************
*     Copyright(c) 2024, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_feat_ctrl.c
   * @brief     enable or disable transport layer feature controlling
   * @author    luke
   * @date      2024-05-15
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2024 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
   */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "stdint.h"
#include "string.h"
#include "trace.h"
#include "app_section.h"
#include "ppt_trans_handle.h"
#include "ppt_trans_feat_ctrl.h"

/*============================================================================*
 *                              Declares Functions
 *============================================================================*/
bool ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEATURES feat);

/*============================================================================*
 *                              Static Functions
 *============================================================================*/
/** feature compatible bitmask */
static uint32_t ppt_trans_feat_macro_en_mask = 0;
/** feature enable bitmask*/
static uint32_t ppt_trans_feat_en_mask = 0;

/*============================================================================*
 *                              Public Functions
 *============================================================================*/
/**
 * @brief  enable or disable specified feature
 *
 * @param  feat - feature to be enable or disabled
 * @param  enable - enable specified feature by passing true, disable it by passing false
 */
void ppt_trans_feat_ctrl_set_feature_status(T_PPT_TRANS_FEATURES feat, bool enable)
{
    if ((ppt_trans_feat_macro_en_mask & BIT(feat)) == 0)
    {
        return;
    }

    APP_PRINT_INFO2("[ppt_trans_feat_ctrl_set_feature_status] set feat %d to %d", feat, enable);
    if (enable == true)
    {
        ppt_trans_feat_en_mask |= BIT(feat);
    }
    else
    {
        ppt_trans_feat_en_mask &= ~BIT(feat);
    }
    return;
}

/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @return bool - true -> feature enabled
 *                false -> feature disabled
 */
bool ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEATURES feat)
{
    if (((ppt_trans_feat_macro_en_mask & BIT(feat)) == 0) ||
        ((ppt_trans_feat_en_mask & BIT(feat)) == 0))
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * @brief  set specified feature as compatible or incompatible.
 *         used to avoid error when application configure wrong macros
 *
 * @param  feat - feature to be enable or disabled
 * @param  enable - enable specified feature by passing true, disable it by passing false
 */
void ppt_trans_feat_ctrl_set_feature_compatible(T_PPT_TRANS_FEATURES feat, bool enable)
{
    uint32_t prev_macro_mask = ppt_trans_feat_en_mask;
    if (enable)
    {
        ppt_trans_feat_macro_en_mask |= BIT(feat);
    }
    else
    {
        ppt_trans_feat_macro_en_mask &= ~BIT(feat);
        ppt_trans_feat_en_mask &= ppt_trans_feat_macro_en_mask;
    }
    APP_PRINT_INFO2("[ppt_trans_feat_ctrl_set_feature_compatible] macro 0x%x -> 0x%x",
                    prev_macro_mask, ppt_trans_feat_en_mask);
}

/**
 * @brief initialization of transport feature ctrl sub-module
 *
 * @param feat - feature enable mask
 *               each bit reprepresents enabling/disable an corresponding feature
 *
 */
void ppt_trans_feat_ctrl_init(uint32_t feat)
{
    uint32_t feature_mask = 0;
#if PPT_TRANS_FEATURE_TRANSPORT_EN
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_TRANSPORT);
#endif

#if PPT_TRANS_FEATURE_PKT_RETRANS_EN
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_PKT_RETRANS);
#endif

#if PPT_TRANS_FEATURE_ADP_RPT_RATE_EN
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_ADP_RATE);
#endif

#if PPT_TRANS_FEATURE_CONFIG_CHAN
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_CONFIG_CHAN);
#endif

#if PPT_TRANS_FEATURE_SUPPORT_CHANNEL_CTRL
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_CHAN_CTRL);
#endif

#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_POS_CTRL);
#endif

#if PPT_TRANS_FEATURE_SUPPORT_16BITS_MOV
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_16BITS_MOV);
#endif

#if PPT_TRANS_SYNC_CTRL_REPORT_STAT
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_DEBUG_REPORT_STAT);
#endif

#if PPT_TRANS_FEATURE_SUPPORT_PAIRING_ID
    feature_mask |= BIT(T_PPT_TRANS_FEAT_CTRL_FEATURE_PAIRING_ID);
#endif

    ppt_trans_feat_macro_en_mask = feature_mask;
    ppt_trans_feat_en_mask = feature_mask & feat;
    APP_PRINT_INFO2("[ppt_trans_feat_ctrl_init] macro enable feat: 0x%x, app enable feat: 0x%x",
                    ppt_trans_feat_macro_en_mask, ppt_trans_feat_en_mask);
}