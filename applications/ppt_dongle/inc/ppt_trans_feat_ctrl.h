/**
*****************************************************************************************
*     Copyright(c) 2024, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      ppt_trans_feat_ctrl.h
   * @brief     proprietary transport layer transport layer feature controlling sub-module
   * @author    luke
   * @date      2024-05-15
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2024 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
   */

#ifndef __PPT_TRANS_FEAT_CTRL_H__
#define __PPT_TRANS_FEAT_CTRL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include "ppt_trans_handle.h"


/*============================================================================*
 *                              Defines
 *============================================================================*/


/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
 * @brief initialization of transport feature ctrl sub-module
 *
 * @param feat - feature enable mask
 *               each bit reprepresents enabling/disable an corresponding feature
 *
 */
void ppt_trans_feat_ctrl_init(uint32_t feat);

/**
 * @brief  set specified feature as compatible or incompatible.
 *         used to avoid error when application configure wrong macros
 *
 * @param  feat - feature to be enable or disabled
 * @param  enable - enable specified feature by passing true, disable it by passing false
 */
void ppt_trans_feat_ctrl_set_feature_compatible(T_PPT_TRANS_FEATURES feat, bool enable);

/**
 * @brief  enable or disable specified feature
 *
 * @param  feat - feature to be enable or disabled
 * @param  enable - enable specified feature by passing true, disable it by passing false
 */
void ppt_trans_feat_ctrl_set_feature_status(T_PPT_TRANS_FEATURES feat, bool enable);

/**
 * @brief  get specified feature enable status
 *
 * @param  feat - feature to be checked
 * @return bool - true -> feature enabled
 *                false -> feature disabled
 */
bool ppt_trans_feat_ctrl_get_feature_status(T_PPT_TRANS_FEATURES feat);

#ifdef __cplusplus
}
#endif

#endif // __PPT_TRANS_FEAT_CTRL_H__

