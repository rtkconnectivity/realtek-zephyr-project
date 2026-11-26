/*
 * Copyright (c) 2026 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * Force-included right after bee5_rom_defines.h (see Makefile) so the vendor
 * header stays untouched while the values below are overridden for this build.
 *
 * CONFIG_LOG_DIRECT_USER is forced to 0 because bee5_rom_defines.h sets it
 * to 1, which makes patch/sys_patch/src/patch_platform_imp.c take this path:
 *
 *   #if (CONFIG_LOG_DIRECT_USER == 1)
 *       fmt_length = VSnprintf(..., fmt, ((const int *)&fmt) + 9);
 *   #else
 *       fmt_length = vsnprintf(..., fmt, arg);
 *   #endif
 *
 * The "+ 9" is not a real va_list. It guesses where the variadic arguments
 * sit on the stack relative to &fmt, and that offset only holds for the
 * stack layout produced by ARMClang v6.22 (the Keil toolchain this code was
 * written for). With GCC (Zephyr SDK) or other optimization levels, the
 * layout differs, so the guessed pointer lands somewhere else, e.g. inside
 * l_msg, and the log arguments are read from the wrong place.
 *
 * With CONFIG_LOG_DIRECT_USER == 0, the code uses the standard
 * va_start/va_list + vsnprintf path, which does not depend on the compiler.
 */
#ifndef ROM_DEFINES_OVERRIDE_H
#define ROM_DEFINES_OVERRIDE_H

#undef CONFIG_LOG_DIRECT_USER
#define CONFIG_LOG_DIRECT_USER 0

#endif /* ROM_DEFINES_OVERRIDE_H */
