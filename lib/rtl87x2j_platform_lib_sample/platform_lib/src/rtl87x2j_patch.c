/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rtl87x2j_patch.h"

#include "patch_clock_imp.h"
#include "patch_modem.h"
#include "patch_pcsm_imp.h"
#include "patch_phy.h"
#include "patch_phy_init.h"
#include "patch_phy_test.h"
#include "patch_phy_utils.h"
#include "patch_platform_imp.h"
#include "patch_pmu_imp.h"
#include "patch_rfc.h"
#include "patch_thermal.h"
#include "patch_thermal_driver.h"

void rtl87x2j_sys_patch_init(void)
{
    sys_patch_ram_pointer_init_pmu();
    sys_patch_xip_pointer_init_pmu();
    sys_patch_data_init_pmu();

    sys_patch_ram_pointer_init_platform();
    sys_patch_xip_pointer_init_platform();
    sys_patch_data_init_platform();

    // add code here
    patch_pointer_init_phy_utils();
    patch_isr_pointer_init_phy_utils();
    patch_data_init_phy_utils();

    patch_pointer_init_rfc();
    patch_isr_pointer_init_rfc();
    patch_data_init_rfc();

    patch_pointer_init_modem();
    patch_isr_pointer_init_modem();
    patch_data_init_modem();

    patch_pointer_init_phy();
    patch_isr_pointer_init_phy();
    patch_data_init_phy();

    patch_pointer_init_phy_test();
    patch_isr_pointer_init_phy_test();
    patch_data_init_phy_test();

    patch_pointer_init_thermal();
    patch_isr_pointer_init_thermal();
    patch_data_init_thermal();

    patch_pointer_init_phy_init();
    patch_data_init_phy_init();

    patch_pointer_init_thermal_driver();
    patch_data_init_thermal_driver();

    patch_pointer_init_clock();
    patch_data_init_clock();
}
