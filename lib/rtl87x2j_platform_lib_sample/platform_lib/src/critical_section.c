/*
 * Copyright (c) 2025 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr-compatible replacement for
 * bee5-zephyr/realtek/kernel/sys/src/critical_section/critical_section.c
 *
 * The original implementation gates on patch_irq_lock / patch_irq_unlock
 * function pointers that are only valid in the mcuboot context.  In the
 * Zephyr application image those pointers live at an address that Zephyr
 * re-uses for thread stacks (filled with 0xAA on CONFIG_INIT_STACKS=y),
 * causing an MPU fault when the eflash driver calls irq_lock().
 *
 * This implementation removes the patch indirection entirely and maps
 * directly onto PRIMASK, which is what the original falls back to when
 * the patch pointers are NULL.  PRIMASK is used instead of Zephyr's
 * BASEPRI-based arch_irq_lock() because platform drivers (eflash, DMA)
 * require ALL interrupts to be masked during critical operations.
 *
 * Symbols exported (match critical_section.h):
 *   uint32_t irq_lock(void)
 *   void     irq_unlock(uint32_t key)
 */

#include "critical_section.h"
/* cpu_setting.h is force-included by the Makefile; it pulls in the CMSIS
 * core header that supplies __get_PRIMASK / __disable_irq / __enable_irq. */

uint32_t irq_lock(void)
{
    uint32_t key = __get_PRIMASK();

    __disable_irq();

    return key;
}

void irq_unlock(uint32_t key)
{
    /* PRIMASK == 0 means interrupts were enabled before irq_lock();
     * only re-enable if that was the case (preserves nesting). */
    if (key == 0)
    {
        __enable_irq();
    }
}
