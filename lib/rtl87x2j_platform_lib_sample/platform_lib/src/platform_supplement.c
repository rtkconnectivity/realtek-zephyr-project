/*
 * Copyright (c) 2025 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stddef.h>
#include <stdint.h>
#include "os_interface.h"
#include "utils.h"
#include "boot_cfg.h"

uint32_t os_timer_max_num_get(void)
{
    return os_interface.os_timer_max_num_get();
}

RTLNUM_Type get_soc_rtl_num(void)
{
    RTLNUM_Type rtl_num =
    {
        .rtl_num1 = RTL_NUM1,
        .rtl_num2 = RTL_NUM2,
    };

    return rtl_num;
}

bool bt_host_enable(void)
{
    return  boot_cfg.vhci_en;
}

uint32_t (*lowerstack_SystemCall_in_platform)(uint32_t opcode, uint32_t param, uint32_t param1,
                                              uint32_t param2);
