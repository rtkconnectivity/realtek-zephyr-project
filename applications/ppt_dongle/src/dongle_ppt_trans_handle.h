/*
 * Copyright (c) 2025 Realtek Semiconductor Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "ppt_trans_handle.h"

/** how many histry packets are kept in memory */
#define DONGLE_PPT_TRANS_HIST_PKT_NUM       32
/** enable printing dongle recving and usb send status */
#define DONGLE_PPT_TRANS_PRINT_STATS_EN     0
/** interval between each packet recv status log, unit ms */
#define DONGLE_PPT_TRANS_PRINT_STATS_ITVL   1000
/** enable absolute position syncing between mouse and dongle */
#define DONGLE_PPT_TRANS_POS_CTRL_EN        (1 && PPT_TRANS_FEATURE_SUPPORT_POS_CTRL)

void dongle_ppt_trans_handle_sync_evt_hook(sync_event_t event);
void dongle_ppt_trans_handle_set_check_release(void);
void dongle_ppt_trans_handle_init(void);
