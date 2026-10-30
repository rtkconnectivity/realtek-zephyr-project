/*
 * Copyright (c) 2025 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * Linux-path-compatible version of the RTL87X2J bootloader
 * boot_patch_define.h. The vendor header uses Keil-style backslash-relative
 * include paths, which GCC on Linux cannot resolve.
 */
#ifndef BOOT_PATCH_DEFINE_H
#define BOOT_PATCH_DEFINE_H

#include "mem_config.h"
#include "mem_cfg_int.h"
#include "flash_map.h"
#include "bee5_rom_defines.h"

#define BOOT_PATCH_FLASH_ADR                BOOT_PATCH_ADDR
#define BOOT_PATCH_FLASH_SIZE               BOOT_PATCH_SIZE

#define BOOT_PATCH_SINGLE_OTA_BANK          0
#define BOOT_PATCH_FEATURE_RAM_CODE         1
#define BOOT_PATCH_RAM_CODE_ENCRYPTED       0
#define BOOT_PATCH_FEATURE_FLASH_SEC        0

#define SUPPORT_SINGLE_OTA_BANK             1

#define SYS_ROM_PCSM_REDUCE_DELAY_TIME      0
#define ZEPHYR_SUPPORT                      1

#define BOOT_PATCH_RAM_TEXT_ADDR            DATA_RAM_BOOT_PATCH_ADDR
#if BOOT_PATCH_FEATURE_RAM_CODE == 1
#define BOOT_PATCH_RAM_TEXT_SIZE            (7 * 1024)
#else
#define BOOT_PATCH_RAM_TEXT_SIZE            (0 * 1024)
#endif

#define BOOT_PATCH_RAM_DATA_ADDR            (BOOT_PATCH_RAM_TEXT_ADDR + BOOT_PATCH_RAM_TEXT_SIZE)
#define BOOT_PATCH_RAM_DATA_SIZE            (DATA_RAM_BOOT_PATCH_SIZE - BOOT_PATCH_RAM_TEXT_SIZE)

#endif /* BOOT_PATCH_DEFINE_H */
