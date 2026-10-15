/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RTL87X2J_PATCH_H
#define RTL87X2J_PATCH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the RTL87X2J sys_patch modules.
 *
 * This function mirrors the initialization of the Realtek ``sys_patch``
 * target. For each patched module (PMU, platform, PHY, RFC, modem, thermal,
 * clock, etc.) it installs the function-pointer and ISR-pointer overrides and
 * initializes the mutable patch data.
 *
 * It must run before any patched ROM/platform API is called, and after the
 * per-image ROM/OCCD configuration has been reconstructed, since the patch
 * data initialization consumes it.
 */
void rtl87x2j_sys_patch_init(void);

#ifdef __cplusplus
}
#endif

#endif /* RTL87X2J_PATCH_H */
