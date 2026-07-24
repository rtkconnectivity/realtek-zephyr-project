/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#if FEATURE_SUPPORT_PROPRIETARY_TRANSPORT
#include "dongle_ppt_trans_handle.h"
#endif
#if PPT_TRANS_FEATURE_SUPPORT_POS_CTRL
#include "ppt_trans_pos_ctrl.h"
#endif
#include "trace.h"

LOG_MODULE_REGISTER(ppt_dongle, 4);

static void global_data_init(void)
{
    app_init_ppt_global_data();
}

int main(void) {
	LOG_DBG("main");
	DBG_DIRECT("MAIN");
	global_data_init();
	dongle_ppt_init();
	dongle_ppt_enable();
}