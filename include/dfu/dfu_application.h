/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PERIPHERAL_APPLICATION__
#define _PERIPHERAL_APPLICATION__

#ifdef __cplusplus
extern "C" {
#endif
#include "app_msg.h"

T_APP_RESULT dfu_profile_callback(T_SERVER_ID service_id, void *p_data);
#ifdef __cplusplus
}
#endif

#endif

