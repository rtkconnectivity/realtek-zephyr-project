#
# Copyright (c) 2025 Realtek Semiconductor Corporation
# SPDX-License-Identifier: Apache-2.0
#
# Makefile for rtl87x2j_platform_lib
#
# This Makefile is invoked by CMake via ExternalProject_Add and receives
# the Zephyr toolchain variables:
#   CC      - ARM C compiler  (e.g. arm-zephyr-eabi-gcc)
#   AR      - archiver        (e.g. arm-zephyr-eabi-ar)
#   CFLAGS  - Zephyr build flags (CPU / FP / optimization / defines)
#
# RTK_BASE can be overridden to point to a different bee5-zephyr checkout:
#   make RTK_BASE=/path/to/bee5-zephyr/realtek ...
#
# Standalone example (Zephyr SDK):
#   make CC=arm-zephyr-eabi-gcc AR=arm-zephyr-eabi-ar \
#        CFLAGS="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -Os -g" \
#        RTK_BASE=/mnt/d/zcode/bee5-zephyr/realtek
#

PREFIX   ?= .
OBJ_DIR  ?= $(PREFIX)/obj
LIB_DIR  ?= $(PREFIX)/lib

# ---------------------------------------------------------------------------
# Toolchain defaults
#
# When invoked by CMake (ExternalProject_Add), CC / AR / CFLAGS are passed
# explicitly on the command line and take full precedence.
#
# For standalone use ("make" with no arguments), fall back to the Zephyr SDK
# cross-compiler.  We check $(origin ...) because make's built-in CC=cc
# cannot be overridden with ?= alone.
# ---------------------------------------------------------------------------
ZEPHYR_SDK ?= /home/zhiyuan/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin

ifeq ($(origin CC),default)
CC := $(ZEPHYR_SDK)/arm-zephyr-eabi-gcc
endif
ifeq ($(origin AR),default)
AR := $(ZEPHYR_SDK)/arm-zephyr-eabi-ar
endif
# Cortex-M33 + FPv5-SP hard-float (matches RTL87x2J Zephyr board config)
CFLAGS ?= -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -Os -g -std=gnu11

# Root of the Realtek bee5-zephyr source tree
RTK_BASE ?= $(HOME)/bee5/realtek

RTK_SOC    := $(RTK_BASE)/soc
RTK_KERNEL := $(RTK_BASE)/kernel
RTK_DRIVER := $(RTK_BASE)/driver
RTK_PATCH  := $(RTK_BASE)/patch

# ---------------------------------------------------------------------------
# Include paths  (mirrors the Keil project include list)
# private_include must precede the vendor bootloader directory so GCC uses the
# Linux-compatible boot_patch_define.h workaround described in README.rst.
# ---------------------------------------------------------------------------
RTK_INCLUDES := \
	-I$(CURDIR)/private_include \
	-I$(CURDIR)/include \
	-I$(RTK_PATCH)/sys_patch/inc \
	-I$(RTK_PATCH)/phy/common/inc \
	-I$(RTK_PATCH)/phy/common/inc_int \
	-I$(RTK_PATCH)/phy/portable/inc \
	-I$(RTK_PATCH)/phy/portable/inc_int \
	-I$(RTK_PATCH)/phy/init_flow \
	-I$(RTK_PATCH)/phy/script \
	-I$(RTK_PATCH)/phy/thermal_driver \
	-I$(RTK_SOC)/flash_proj/bootloader/inc \
	-I$(RTK_KERNEL)/soc/inc/rtl87x2j \
	-I$(RTK_SOC)/boot/rom_boot \
	-I$(RTK_SOC)/boot/eflash_boot \
	-I$(RTK_SOC)/rxi300 \
	-I$(RTK_SOC)/staging \
	-I$(RTK_KERNEL)/arch/arm/cmsis/CMSIS_5-5.9.0/CMSIS/Core/Include \
	-I$(RTK_KERNEL)/arch/common \
	-I$(RTK_KERNEL)/logging/src \
	-I$(RTK_KERNEL)/logging/src/rtl87x2j \
	-I$(RTK_KERNEL)/logging/inc \
	-I$(RTK_KERNEL)/logging/inc/rtl87x2j \
	-I$(RTK_KERNEL)/memory/inc/rtl87x2j \
	-I$(RTK_KERNEL)/memory/inc/rtl87x2j/src \
	-I$(RTK_KERNEL)/memory/src/rtl87x2j \
	-I$(RTK_KERNEL)/util/inc \
	-I$(RTK_KERNEL)/util/inc/rtl87x2j \
	-I$(RTK_KERNEL)/debug/src \
	-I$(RTK_KERNEL)/debug/inc \
	-I$(RTK_KERNEL)/debug/inc/rtl87x2j \
	-I$(RTK_KERNEL)/sys/inc \
	-I$(RTK_KERNEL)/staging/rtl87x2j/inc \
	-I$(RTK_KERNEL)/linker/inc \
	-I$(RTK_BASE)/boards/boot/inc/rtl87x2j \
	-I$(RTK_BASE)/boards/config/flash_map/rtl87x2j \
	-I$(RTK_BASE)/boards/config/memory/rtl87x2j \
	-I$(RTK_SOC)/driver \
	-I$(RTK_SOC)/driver/dma/inc \
	-I$(RTK_SOC)/driver/irq/inc \
	-I$(RTK_SOC)/driver/rcc/inc \
	-I$(RTK_SOC)/driver/wdt/inc \
	-I$(RTK_SOC)/driver/wdt/src/device/rtl87x2j \
	-I$(RTK_SOC)/driver/uart/inc \
	-I$(RTK_SOC)/driver/timer/inc \
	-I$(RTK_SOC)/driver/tm/inc \
	-I$(RTK_SOC)/driver/gtc/inc \
	-I$(RTK_SOC)/driver/rap/inc \
	-I$(RTK_SOC)/driver/rtc/inc \
	-I$(RTK_SOC)/driver/debug/inc \
	-I$(RTK_SOC)/driver/pinmux/inc \
	-I$(RTK_SOC)/driver/pinmux/inc/rtl87x2j \
	-I$(RTK_SOC)/cpu_component \
	-I$(RTK_SOC)/clock/eflash_boot \
	-I$(RTK_SOC)/clock/rom_boot \
	-I$(RTK_SOC)/otp \
	-I$(RTK_SOC)/phy/common/inc \
	-I$(RTK_SOC)/phy/common/inc_int \
	-I$(RTK_SOC)/phy/portable/inc \
	-I$(RTK_SOC)/phy/portable/inc_int \
	-I$(RTK_SOC)/phy/init_flow \
	-I$(RTK_SOC)/phy/script \
	-I$(RTK_SOC)/phy/thermal_driver \
	-I$(RTK_SOC)/pm/common/inc \
	-I$(RTK_SOC)/pm/common/inc_int \
	-I$(RTK_SOC)/pm/portable/inc \
	-I$(RTK_SOC)/pm/power_manager_unit_platform \
	-I$(RTK_SOC)/mp/inc \
	-I$(RTK_SOC)/hci/inc \
	-I$(RTK_SOC)/sch_pln \
	-I$(RTK_SOC)/power/pck600 \
	-I$(RTK_SOC)/power/wakeup \
	-I$(RTK_SOC)/power/pmu \
	-I$(RTK_SOC)/amu \
	-I$(RTK_SOC)/secure_boot \
	-I$(RTK_DRIVER)/crypto/src/crypto_interface \
	-I$(RTK_DRIVER)/crypto/src/crypto_interface/symmetric \
	-I$(RTK_DRIVER)/crypto/src/crypto_interface/hash \
	-I$(RTK_DRIVER)/crypto/src/crypto_interface/asymmetric \
	-I$(RTK_DRIVER)/crypto/src/crypto_interface/rng \
	-I$(RTK_DRIVER)/crypto/src/hardware_driver/cn4/symmetric \
	-I$(RTK_DRIVER)/crypto/src/hardware_driver/cn4/hash \
	-I$(RTK_DRIVER)/fmc/inc/rtl87x2j \
	-I$(RTK_DRIVER)/fmc/src/eflash/rtl87x2j \
	-I$(RTK_DRIVER)/fmc/src/controller_driver/eflash_controller/rtl87x2j \
	-I$(RTK_DRIVER)/fmc/src/fmc_platform/rtl87x2j

# ---------------------------------------------------------------------------
# Compile-time defines  (mirrors Keil project defines for Zephyr target)
# ---------------------------------------------------------------------------
RTK_DEFINES := \
	-DCONFIG_SOC_SERIES_RTL87X2J \
	-DBOOTLOADER_ENABLE_SECURE_BOOT=0 \
	-DCONFIG_LOGGING_MINIMIZE_BOOT_LOG=0 \
	-DCONFIG_LOGGING_BUFFER_LOG=1 \
	-DCONFIG_REALTEK_SINGLE_BANK_DFU=0 \
	-DZEPHYR_SUPPORT=1

# Force-include headers (mirrors Keil "--include" flags).
# private_include/ overrides the vendor boot_patch_define.h with a Linux version.
RTK_FORCE_INCLUDES := \
	-include bee5_rom_defines.h \
	-include cpu_setting.h \
	-include boot_patch_define.h

# sys_patch_define.h contains Windows-only relative includes. This archive needs
# only these feature switches, so suppress that header while compiling patches.
PATCH_CFLAGS := \
	-DSYS_PATCH_DEFINE_H \
	-DSYS_PATCH_APPLY_FT_OTP_PARAS=1 \
	-DSYS_PATCH_XTAL_AAC_CAL=1

# Combined extra RTK flags (prepended to whatever CFLAGS was passed in)
RTK_CFLAGS := $(RTK_INCLUDES) $(RTK_DEFINES) $(RTK_FORCE_INCLUDES) \
	-fno-builtin

# -Wno-expansion-to-defined: vendor headers (rfc_int.h, modem_portmacro.h) use
# defined() inside macro arguments, which is non-portable but intentional.
# Must come AFTER $(CFLAGS): Zephyr's warning_base adds -Wexpansion-to-defined,
# and the last occurrence on the command line wins.
RTK_LATE_CFLAGS := -Wno-expansion-to-defined

# ---------------------------------------------------------------------------
# Source files
# Keil project list minus: secure_boot / DFU / crypto drivers /
# boot_patch_entry.c (replaced by src/rtl87x2j_platform_init.c).
# system_init.c IS included — it defines hal_setup_hardware, hal_setup_cpu,
# si_flow_data_init and other symbols called from rtl87x2j_platform_init.c.
# ---------------------------------------------------------------------------

# Our platform init wrapper and compatibility stubs (this library's own code)
SRCS_LOCAL := \
	src/rtl87x2j_platform_init.c \
	src/rtl87x2j_patch.c \
	src/platform_supplement.c \
	src/critical_section.c

# Source functions from patch/sys_patch/sys_patch.uvprojx. The standalone
# sys_patch image entry/header are intentionally omitted; Zephyr calls the
# exported rtl87x2j_patch_* APIs instead.
PATCH_SRCS := \
	$(RTK_PATCH)/sys_patch/src/patch_pmu_imp.c \
	$(RTK_PATCH)/sys_patch/src/patch_pcsm_imp.c \
	$(RTK_PATCH)/sys_patch/src/patch_platform_imp.c \
	$(RTK_PATCH)/sys_patch/src/patch_clock_imp.c \
	$(RTK_PATCH)/phy/common/src/patch_modem.c \
	$(RTK_PATCH)/phy/common/src/patch_phy.c \
	$(RTK_PATCH)/phy/common/src/patch_phy_test.c \
	$(RTK_PATCH)/phy/common/src/patch_phy_utils.c \
	$(RTK_PATCH)/phy/common/src/patch_rfc.c \
	$(RTK_PATCH)/phy/common/src/patch_thermal.c \
	$(RTK_PATCH)/phy/portable/src/patch_modem_port.c \
	$(RTK_PATCH)/phy/portable/src/patch_phy_port.c \
	$(RTK_PATCH)/phy/portable/src/patch_phy_test_port.c \
	$(RTK_PATCH)/phy/portable/src/patch_phy_utils_port.c \
	$(RTK_PATCH)/phy/portable/src/patch_rfc_port.c \
	$(RTK_PATCH)/phy/portable/src/patch_thermal_port.c \
	$(RTK_PATCH)/phy/init_flow/patch_phy_init.c \
	$(RTK_PATCH)/phy/script/patch_phy_script.c \
	$(RTK_PATCH)/phy/thermal_driver/patch_thermal_driver.c


# Keep all patch objects in one relocatable object. A normal archive extracts
# members on demand; without this step, referencing rtl87x2j_patch_pointer_init
# would not extract the pointer-variable definitions that are otherwise reached
# only through runtime calls from ROM code.
PATCH_BUNDLE_OBJ := $(OBJ_DIR)/rtl87x2j_patch_bundle.o

# Boot / system init
# mmu_ctrl.c  → REMOVED: mmu_setup not referenced by any kept object
SRCS_BOOT := \
	$(RTK_SOC)/boot/eflash_boot/system_init.c \
	$(RTK_SOC)/boot/eflash_boot/image_info.c \
	$(RTK_SOC)/boot/eflash_boot/boot_cfg.c \
	$(RTK_SOC)/boot/eflash_boot/rot_ctrl.c

# CPU component
SRCS_CPU := \
	$(RTK_SOC)/cpu_component/mpu.c \
	$(RTK_SOC)/cpu_component/fpu.c \
	$(RTK_SOC)/cpu_component/cpu_cfg.c

# Peripheral drivers
SRCS_DRV := \
	$(RTK_SOC)/driver/debug/src/device/rtl87x2j/debug_port.c \
	$(RTK_SOC)/driver/debug/src/device/rtl87x2j/patch_debug_port_function.c \
	$(RTK_SOC)/driver/dma/src/device/rtl_common/rtl_dma.c \
	$(RTK_SOC)/driver/dma/src/device/rtl_common/patch_dma_function.c \
	$(RTK_SOC)/driver/dma/src/device/rtl87x2j/rtl87x2j_dma.c \
	$(RTK_SOC)/driver/rtc/src/device/rtl_common/rtl_grtc.c \
	$(RTK_SOC)/driver/rtc/src/device/rtl_common/patch_grtc_function.c \
	$(RTK_SOC)/driver/rtc/src/device/rtl87x2j/rtl87x2j_grtc.c \
	$(RTK_SOC)/driver/gtc/src/device/rtl_common/rtl_gtc.c \
	$(RTK_SOC)/driver/gtc/src/device/rtl_common/patch_gtc_function.c \
	$(RTK_SOC)/driver/gtc/src/device/rtl87x2j/rtl87x2j_gtc.c \
	$(RTK_SOC)/driver/irq/src/device/rtl87x2j/rtl_nvic.c \
	$(RTK_SOC)/driver/irq/src/device/rtl87x2j/patch_nvic_function.c \
	$(RTK_SOC)/driver/rap/src/device/rtl_common/rtl_rap.c \
	$(RTK_SOC)/driver/rap/src/device/rtl_common/patch_rap_function.c \
	$(RTK_SOC)/driver/rap/src/device/rtl87x2j/rtl87x2j_rap.c \
	$(RTK_SOC)/driver/timer/src/device/rtl_common/rtl_timer.c \
	$(RTK_SOC)/driver/timer/src/device/rtl_common/patch_timer_function.c \
	$(RTK_SOC)/driver/timer/src/device/rtl87x2j/rtl87x2j_timer.c \
	$(RTK_SOC)/driver/tm/src/device/rtl_common/rtl_tm.c \
	$(RTK_SOC)/driver/tm/src/device/rtl_common/patch_tm_function.c \
	$(RTK_SOC)/driver/tm/src/device/rtl87x2j/rtl87x2j_tm.c \
# rtl_wakeup.c and patch_wakeup_function.c excluded:
#   - rtl_wakeup.c includes patch_wakeup_function.h which has a
#     trailing-space bug in its #include "rtl_pinmux.h " directive
#   - wakeup_init() is only called in the ZEPHYR_SUPPORT=0 path of
#     main_full(), which we replace with a stub

# Memory controller
SRCS_MEM := \
	$(RTK_KERNEL)/memory/src/rtl87x2j/ram_pwr_ctrl.c \
	$(RTK_KERNEL)/memory/src/rtl87x2j/mem_ctrl_cfg.c

# RXI300 bus fabric
SRCS_RXI300 := \
	$(RTK_SOC)/rxi300/rxi300_ctrl.c \
	$(RTK_SOC)/rxi300/rxi300_timeout.c

# Utilities / system services
# crc16btx.c   → REMOVED: btxfcs/crc16_ibm not referenced by any kept object
SRCS_UTIL := \
	$(RTK_KERNEL)/util/src/occd_parser.c \
	$(RTK_KERNEL)/sys/src/sys_list.c \
	$(RTK_KERNEL)/sys/src/sys_reset/sys_reset_port/rtl87x2j/sys_reset_nvic.c \
	$(RTK_KERNEL)/sys/src/timestamp/rtl87x2j/timestamp.c

# Logging subsystem
SRCS_LOG := \
	$(RTK_KERNEL)/logging/src/log_format.c \
	$(RTK_KERNEL)/logging/src/log_format_patch_ptr.c \
	$(RTK_KERNEL)/logging/src/rtl87x2j/log_format_port.c \
	$(RTK_KERNEL)/logging/src/rtl87x2j/log_uart_dma.c \
	$(RTK_KERNEL)/logging/src/rtl87x2j/pingpong_buffer.c \
	$(RTK_KERNEL)/logging/src/rtl87x2j/log_direct_dma.c

# Debug / assert
SRCS_DBG := \
	$(RTK_KERNEL)/debug/src/assert_handler.c

# PHY (RF / modem / thermal)
# These sources use GNU empty variadic macro handling in phy_utils.h, so this
# external library is compiled as GNU C17 by CMakeLists.txt.
SRCS_PHY := \
	$(RTK_SOC)/phy/portable/src/modem_port.c \
	$(RTK_SOC)/phy/portable/src/phy_port.c \
	$(RTK_SOC)/phy/portable/src/phy_test_port.c \
	$(RTK_SOC)/phy/portable/src/phy_utils_port.c \
	$(RTK_SOC)/phy/portable/src/rfc_port.c \
	$(RTK_SOC)/phy/portable/src/thermal_port.c \
	$(RTK_SOC)/phy/common/src/modem.c \
	$(RTK_SOC)/phy/common/src/phy.c \
	$(RTK_SOC)/phy/common/src/phy_test.c \
	$(RTK_SOC)/phy/common/src/phy_utils.c \
	$(RTK_SOC)/phy/common/src/rfc.c \
	$(RTK_SOC)/phy/common/src/thermal.c \
	$(RTK_SOC)/phy/init_flow/phy_init.c \
	$(RTK_SOC)/phy/script/phy_script.c \
	$(RTK_SOC)/phy/thermal_driver/thermal_driver.c \
	$(RTK_SOC)/hci/src/hci_phy_power_vendor.c

# Power management
# wakeup_init, amu_init, amu_script_init, power_manager_init, platform_pm_init
# are all in the PROVIDE() list → include their source files.
SRCS_PM := \
	$(RTK_SOC)/power/pck600/ppu.c \
	$(RTK_SOC)/power/pck600/pck600.c \
	$(RTK_SOC)/power/wakeup/wakeup.c \
	$(RTK_SOC)/power/pmu/pmu.c \
	$(RTK_SOC)/power/pmu/pcsm.c \
	$(RTK_SOC)/pm/common/src/power_manager.c \
	$(RTK_SOC)/pm/portable/src/power_manager_port.c \
	$(RTK_SOC)/pm/power_manager_unit_platform/power_manager_unit_platform.c \
	$(RTK_SOC)/amu/amu.c

# Clock management
# clock_manager_power_interface.c → REMOVED: none of its symbols (clock_check_
#   request_handle_valid, clock_queue_in_request, …) are referenced by any
#   kept object or appear in the PROVIDE() list.
SRCS_CLK := \
	$(RTK_SOC)/clock/eflash_boot/clock_manager_cal.c \
	$(RTK_SOC)/clock/eflash_boot/clock_manager.c

# Staging / platform interface shims
# patch_os_timer_max_num_get (the only symbol from platform_interface.c used by
# the Zephyr integration) is provided as a NULL-initialised stub in
# src/os_timer_patch_stub.c; osif_zephyr_patch.c assigns the real function
# pointer during os_zephyr_patch_init().
SRCS_STAGING :=

# OS interface
# All os_* symbols are in the PROVIDE() list → the .a must export them.
SRCS_OS := \
	$(RTK_KERNEL)/os/osif/src/os_interface.c \
	$(RTK_KERNEL)/os/osif/src/os_mem.c \
	$(RTK_KERNEL)/os/osif/src/os_msg.c \
	$(RTK_KERNEL)/os/osif/src/os_sched.c \
	$(RTK_KERNEL)/os/osif/src/os_sync.c \
	$(RTK_KERNEL)/os/osif/src/os_task.c \
	$(RTK_KERNEL)/os/osif/src/os_timer.c \
	$(RTK_KERNEL)/os/osif/src/os_pm.c

# MP test / HCI vendor commands
SRCS_MP_HCI := \
	$(RTK_SOC)/mp/src/platform_mp_cmd.c

# Schedule planner
SRCS_SCH := \
	$(RTK_SOC)/sch_pln/schedule_plan.c

# OTP driver
SRCS_OTP := \
	$(RTK_SOC)/otp/ft_otp_driver.c

# eFlash / FMC
# eflash_erase/read/write are in the PROVIDE() list → include eflash.c.
SRCS_FMC := \
	$(RTK_DRIVER)/fmc/src/eflash/rtl87x2j/eflash_driver.c \
	$(RTK_DRIVER)/fmc/src/eflash_api/eflash.c

# AES interface used by rot_ctrl.c, plus its CN4 hardware implementation.
# SHA-2 interface and CN4 engine are used by the Zephyr PSA hardware
# accelerator driver (sha2_init, sha2_start, sha2_cpu_update, ...).
SRCS_CRYPTO := \
	$(RTK_DRIVER)/crypto/src/crypto_interface/symmetric/aes_interface.c \
	$(RTK_DRIVER)/crypto/src/hardware_driver/cn4/symmetric/aes_engine_driver.c \
	$(RTK_DRIVER)/crypto/src/crypto_interface/hash/sha2_interface.c \
	$(RTK_DRIVER)/crypto/src/hardware_driver/cn4/hash/sha2_engine_driver.c

# All sources
SRCS := \
	$(SRCS_LOCAL) \
	$(PATCH_SRCS) \
	$(SRCS_BOOT) \
	$(SRCS_CPU) \
	$(SRCS_DRV) \
	$(SRCS_MEM) \
	$(SRCS_RXI300) \
	$(SRCS_UTIL) \
	$(SRCS_LOG) \
	$(SRCS_DBG) \
	$(SRCS_PHY) \
	$(SRCS_PM) \
	$(SRCS_CLK) \
	$(SRCS_STAGING) \
	$(SRCS_OS) \
	$(SRCS_MP_HCI) \
	$(SRCS_SCH) \
	$(SRCS_OTP) \
	$(SRCS_FMC) \
	$(SRCS_CRYPTO)

# ---------------------------------------------------------------------------
# Object file derivation
# Flatten the source path to a unique filename in OBJ_DIR:
#   soc/driver/dma/src/device/rtl_common/rtl_dma.c
#   → $(OBJ_DIR)/soc_driver_dma_src_device_rtl_common_rtl_dma.o
#
# Local files (under $(CURDIR)) strip the CURDIR prefix first.
# ---------------------------------------------------------------------------
src_to_obj = $(OBJ_DIR)/$(subst /,_,$(subst $(RTK_BASE)/,,$(subst $(CURDIR)/,,$(1)))).o

PATCH_OBJS := $(foreach s,$(PATCH_SRCS),$(call src_to_obj,$(s)))
OBJS := $(foreach s,$(filter-out $(PATCH_SRCS),$(SRCS)),$(call src_to_obj,$(s))) \
	$(PATCH_BUNDLE_OBJ)

.PHONY: all clean

all: $(LIB_DIR)/librtl87x2j_platform_lib.a

$(LIB_DIR)/librtl87x2j_platform_lib.a: $(OBJS)
	@mkdir -p $(LIB_DIR)
	@$(AR) -rcs $@ $^
	@echo "[rtl87x2j_platform_lib] Built: $@"

$(PATCH_BUNDLE_OBJ): $(PATCH_OBJS)
	@mkdir -p $(OBJ_DIR)
	@$(CC) -r -o $@ $^

# Generic compile rule — one per source file, evaluated by $(foreach)
define compile_rule
$(call src_to_obj,$(1)): $(1)
	@mkdir -p $(OBJ_DIR)
	@$(CC) -c $(RTK_CFLAGS) $(CFLAGS) $(RTK_LATE_CFLAGS) $(2) \
	    -MD -MF $$(patsubst %.o,%.d,$(call src_to_obj,$(1))) \
	    -o $(call src_to_obj,$(1)) $(1)
endef

$(foreach s,$(filter-out $(PATCH_SRCS) $(RTK_KERNEL)/logging/src/log_format.c,$(SRCS)), \
	$(eval $(call compile_rule,$(s))))
$(foreach s,$(PATCH_SRCS),$(eval $(call compile_rule,$(s),$(PATCH_CFLAGS))))
# log_format.c returns NULL from a uint32_t function; keep the vendor workaround
# local so new int-conversion bugs in the rest of the library remain errors.
$(eval $(call compile_rule,$(RTK_KERNEL)/logging/src/log_format.c,-Wno-error=int-conversion))

# Include auto-generated dependency files (silently absent on first build)
-include $(OBJS:.o=.d) $(PATCH_OBJS:.o=.d)

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR)
