/*
 * Copyright (c) 2025 Realtek Semiconductor Corporation
 * SPDX-License-Identifier: Apache-2.0
 *
 * RTL87x2J platform initializer for Zephyr / MCUboot.
 *
 * Derived from:
 *   realtek/soc/flash_proj/bootloader/src/boot_patch_entry.c  (boot_patch_entry)
 *   realtek/soc/boot/eflash_boot/system_init.c                (SystemInit_zephyr)
 *
 * Omissions vs. the original Realtek bootloader:
 *   - secure_boot_entry()  : handled by MCUboot image verification
 *   - image_entry()        : handled by MCUboot before jumping to Zephyr
 *   - ram_init()           : Keil scatter-load; not needed — Zephyr startup
 *                            handles .data copy and .bss zero-init
 *   - timestamp_init()     : TIMESTAMP_IRQ is registered on the Zephyr side
 *   - log subsystem init   : deferred to the Zephyr application layer
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "rtl87x2j_platform_init.h"

/* SoC / platform headers */
#include "aon_reg.h"
#include "platform_reg.h"
#include "eflash_driver.h"
#include "rxi300_ctrl.h"
#include "occd_parser.h"
#include "rot_ctrl.h"
#include "rot_preliminary.h"
#include "otp_api.h"
#include "otp_table.h"
#include "image_info.h"
#include "boot_cfg.h"
#include "dbg_cfg.h"
#include "rom_cfg.h"
#include "clock_manager_cal.h"
#include "clock_manager.h"
#include "cpu_cfg.h"
#include "ft_otp_driver.h"
#include "pck600.h"
#include "schedule_plan.h"
#include "pmu.h"
#include "rtl_rcc.h"
#include "rtl_rap.h"
#include "rtl_tm.h"
#include "mpu.h"
#include "fpu.h"
#include "ram_pwr_ctrl.h"
#include "rxi300_timeout.h"
#include "system_init.h"
#include "soc_log.h"
#include "assert_handler.h"
#include "utils.h"
#include "power_manager.h"
#include "wakeup.h"
#include "phy.h"
#include "thermal_driver.h"
#include "amu.h"
#include "rtl87x2j_patch.h"
#include "power_manager_unit_platform.h"

/* -------------------------------------------------------------------------
 * MBISR RAM repair
 *
 * Verbatim from boot_patch_entry.c, made file-local (static).
 *
 * MBISR (Memory Built-In Self-Repair) uses per-row repair metadata stored in
 * the eFlash NVR (Non-Volatile Register) region to remap defective SRAM rows
 * to spare rows.  Must run before any code accesses the data SRAMs.
 *
 * Repair info layout in eFlash NVR (one 32-bit word at offset +5*4):
 *   bits  [5: 0]  – data SRAM 0 repair info  (6-bit field)
 *   bits  [13: 8]  – data SRAM 1 repair info  (6-bit field)
 *   bits  [21:16]  – data SRAM 2 repair info  (6-bit field)
 *   bits  [30:24]  – buffer SRAM 0 repair info (7-bit field)
 * ----------------------------------------------------------------------- */

/** Base address of the packed MBISR repair-info word in eFlash NVR. */
#define MBISR_REPAIR_INFO_START_ADDR            (EFLASH_AUTO_MODE_NVR_CFG_ADDR + 0x5 * 4)

/* Bit-field offsets within the packed repair-info word. */
#define MBISR_DATA_RAM0_REPAIR_INFO_BIT_OFFSET  (0)
#define MBISR_DATA_RAM1_REPAIR_INFO_BIT_OFFSET  (8)
#define MBISR_DATA_RAM2_REPAIR_INFO_BIT_OFFSET  (16)
#define MBISR_BUFFER_RAM_REPAIR_INFO_BIT_OFFSET (24)

/* Bit-field masks (data SRAMs are 6-bit; buffer SRAM is 7-bit). */
#define MBISR_DATA_RAM_REPAIR_INFO_BIT_MASK     (0x3F)
#define MBISR_BUFFER_RAM_REPAIR_INFO_BIT_MASK   (0x7F)

/**
 * mbisr_with_repair_info - Apply MBISR row-repair to data and buffer SRAMs.
 *
 * Three-phase sequence:
 *   Phase 1: Read repair info from eFlash NVR and write it to platform regs.
 *   Phase 2: Pulse the MBISR clock and enable the repair function; hardware
 *            remaps defective rows during the 2 µs window.
 *   Phase 3: Disable the MBISR clock and repair enable (normal SRAM operation
 *            resumes with repaired row mapping in effect).
 */
static void mbisr_with_repair_info(void)
{
    /* ---- Phase 1: read repair info from eFlash NVR ---- */

    /* All four RAM repair fields are packed into a single 32-bit NVR word. */
    uint32_t all_ram_mbisr_repair_info = HAL_READ32(MBISR_REPAIR_INFO_START_ADDR, 0);

    uint32_t data_sram_0_repair_info   = (all_ram_mbisr_repair_info >>
                                          MBISR_DATA_RAM0_REPAIR_INFO_BIT_OFFSET)
                                         & MBISR_DATA_RAM_REPAIR_INFO_BIT_MASK;
    uint32_t data_sram_1_repair_info   = (all_ram_mbisr_repair_info >>
                                          MBISR_DATA_RAM1_REPAIR_INFO_BIT_OFFSET)
                                         & MBISR_DATA_RAM_REPAIR_INFO_BIT_MASK;
    uint32_t data_sram_2_repair_info   = (all_ram_mbisr_repair_info >>
                                          MBISR_DATA_RAM2_REPAIR_INFO_BIT_OFFSET)
                                         & MBISR_DATA_RAM_REPAIR_INFO_BIT_MASK;
    uint32_t buffer_sram_0_repair_info = (all_ram_mbisr_repair_info >>
                                          MBISR_BUFFER_RAM_REPAIR_INFO_BIT_OFFSET)
                                         & MBISR_BUFFER_RAM_REPAIR_INFO_BIT_MASK;

    /*
     * Pack the three data-SRAM fields (each 6 bits) into the single
     * bisr_remap_sig_data_ram bitfield expected by the platform register:
     *   [17:12] = SRAM0, [11:6] = SRAM1, [5:0] = SRAM2
     */
    uint32_t data_sram_repair_info = (data_sram_0_repair_info << 12) |
                                     (data_sram_1_repair_info << 6)  |
                                      data_sram_2_repair_info;

    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_DATA_SRAM_CTRL, bisr_remap_sig_data_ram,
                                data_sram_repair_info);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_BUF_SRAM_CTRL, bisr_remap_sig_buffer_ram,
                                buffer_sram_0_repair_info);

    /* ---- Phase 2: trigger the MBISR repair sequence ---- */

    /* Step 2-1: enable the MBISR clock for data and buffer RAM domains. */
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_BIST_MODE, bist_mode_data_ram, 1);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_BT_BIST_MODE, bist_mode_buffer_ram, 1);

    /* Step 2-2: de-assert remap_rstn to bring the repair logic out of reset. */
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_DATA_SRAM_CTRL, bisr_remap_rstn_data_ram, 1);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_BUF_SRAM_CTRL, bisr_remap_rstn_buffer_ram, 1);

    /* Step 2-3: load the repair fuse data into the SRAM row-remap circuits. */
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_DATA_SRAM_CTRL, bisr_load_fuse_data_ram, 1);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_BUF_SRAM_CTRL, bisr_load_fuse_buffer_ram, 1);

    /* Step 2-4: wait for the repair operation to complete (~2 µs worst case). */
    platform_delay_us(2);

    /* ---- Phase 3: disable MBISR (SRAM now operates with repaired mapping) ---- */

    /* Step 3-1: gate the MBISR clock. */
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_BIST_MODE, bist_mode_data_ram, 0);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_BT_BIST_MODE, bist_mode_buffer_ram, 0);

    /* Step 3-2: de-assert the repair-load enable (latches are transparent). */
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_DATA_SRAM_CTRL, bisr_load_fuse_data_ram, 0);
    PLATFORM_REG_WRITE_BITFIELD(PLATFORM_REG_MBISR_BUF_SRAM_CTRL, bisr_load_fuse_buffer_ram, 0);
}

/* -------------------------------------------------------------------------
 * OCCD ROM-config parsing
 *
 * Verbatim from boot_patch_entry.c, made file-local (static).
 *
 * The OCCD (On-Chip Configuration Data) flash partition holds a serialised
 * stream of module configuration blobs.  parse_boot_rom_from_occd() walks
 * all MODULE_ID_BOOT_MAX entries and deserialises each blob directly into the
 * corresponding in-RAM config struct (dbg_cfg, boot_cfg, clk_cfg, …).
 * ----------------------------------------------------------------------- */

/**
 * boot_rom_cfg_info - Maps each boot-phase module ID to its RAM config struct.
 *
 * The array is indexed by MODULE_ID_* and must cover the range
 * [MODULE_ID_DBG, MODULE_ID_BOOT_MAX).  A compile-time size check inside
 * parse_boot_rom_from_occd() guards against accidental mis-sizing.
 */
static const MODULE_INFO boot_rom_cfg_info[MODULE_ID_BOOT_MAX] =
{
    [MODULE_ID_DBG] = {
        .base = (uint32_t *) &dbg_cfg,
        .size = sizeof(dbg_cfg),
    },
    [MODULE_ID_BOOT_ROM] = {
        .base = (uint32_t *) &boot_rom_cfg,
        .size = sizeof(boot_rom_cfg),
    },
    [MODULE_ID_BOOT] = {
        .base = (uint32_t *) &boot_cfg,
        .size = sizeof(boot_cfg),
    },
    [MODULE_ID_CLOCK_CAL] = {
        .base = (uint32_t *) &clk_cal_cfg,
        .size = sizeof(clk_cal_cfg),
    },
    [MODULE_ID_CLOCK] = {
        .base = (uint32_t *) &clk_cfg,
        .size = sizeof(clk_cfg),
    },
    [MODULE_ID_CPU] = {
        .base = (uint32_t *) &cpu_cfg,
        .size = sizeof(cpu_cfg),
    },
#if (CONFIG_REALTEK_SINGLE_BANK_DFU == 1)
    /* DFU config is only present in single-bank DFU builds. */
    [MODULE_ID_DFU] = {
        .base = (uint32_t *) &dfu_cfg,
        .size = sizeof(dfu_cfg),
    },
#endif
};

/**
 * parse_boot_rom_from_occd - Deserialise ROM module configs from OCCD flash.
 *
 * @param occd_payload_addr  Physical address of the OCCD payload in flash.
 * @param occd_payload_size  Byte length of the OCCD payload.
 *
 * Iterates MODULE_ID_DBG … MODULE_ID_BOOT_MAX−1 and calls parse_sys_cfg_to_mem()
 * for each module.  parse_sys_cfg_to_mem() returns PARSE_ERROR_NO_CONFIG when
 * no blob exists for that module, which is not treated as an error.  Any
 * other non-zero return code indicates a genuine parsing failure and is logged.
 *
 * The leading size-check ensures boot_rom_cfg_info covers exactly the expected
 * range; if the array is accidentally mis-sized the function is a no-op.
 */
static void parse_boot_rom_from_occd(uint32_t occd_payload_addr, uint32_t occd_payload_size)
{
    /* Guard: boot_rom_cfg_info must cover every entry in [MODULE_ID_DBG, MODULE_ID_BOOT_MAX). */
    if ((sizeof(boot_rom_cfg_info) / sizeof(MODULE_INFO)) ==
        MODULE_ID_BOOT_MAX - MODULE_ID_DBG)
    {
        PARSING_PARAMETER_FORMAT parsing_info = {
            .signature = SYS_CFG_SIGNATURE,
            .max_size  = occd_payload_size,
            .sys_cfg   = (void *)(occd_payload_addr),
        };

        for (size_t i = MODULE_ID_DBG; i < MODULE_ID_BOOT_MAX; i++)
        {
            if (parse_sys_cfg_to_mem(&parsing_info, i,
                                     (uint8_t *)boot_rom_cfg_info[i].base,
                                     boot_rom_cfg_info[i].size) > PARSE_ERROR_NO_CONFIG)
            {
                DIRECT_LOG("Parsing Error! CFG_Header:0x%x", i);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Public entry points
 * ----------------------------------------------------------------------- */

void rtl87x2j_platform_reset_init(void)
{
        /* E1. RAM repair: apply MBISR fuse data from eFlash NVR to remap defective
     *     rows in data SRAMs (0/1/2) and the buffer SRAM before anything else
     *     accesses those memories. */
    mbisr_with_repair_info();
}
/**
 * rtl87x2j_platform_early_init - Pre-kernel SoC platform initialization.
 *
 * Must be called before the Zephyr scheduler starts (or before MCUboot hands
 * off to the Zephyr image).  Interrupts may be disabled throughout.
 * No Zephyr OS services are used.
 *
 * See rtl87x2j_platform_init.h for the full step-by-step sequence.
 */
void rtl87x2j_platform_early_init(void)
{
    /* E2. Enable the ROM assertion handler so assert() failures produce a
     *     diagnostic instead of silently hanging. */
    assert_enable_set(true);

    /* E3. Wire up eFlash driver function pointers for IRQ lock / unlock.
     *     Required before any code that calls the eFlash API. */
    eflash_assign_func_pointer();

    /* E4. Initialize the RXI300 AHB bus fabric and update its IRQ routing.
     *     Skipped when the AON OTP IS_RXI300_DISABLE flag indicates the bus
     *     fabric is not present on this die variant. */
    if (AON_REG_READ_BITFIELD(AON_REG_FW_GENERAL_REG6X, IS_RXI300_DISABLE) == 0)
    {
        rxi300_init(); /* also updates RXI300_IRQn vector */
    }

    /* E5. Deserialise ROM module configurations (debug, boot, clock, CPU, …)
     *     from the OCCD flash partition into their in-RAM config structs.
     *     Config data overrides ROM defaults before any subsystem reads them. */
    parse_boot_rom_from_occd(image_payload_addr_get(IMG_OCCD),
                             image_payload_size_get(IMG_OCCD));

    /* E6. Root-of-Trust (ROT): load hardware root keys, configure the Factory
     *     Provisioned Key (FPK), and apply general system-level security
     *     controls (e.g. debug-port policy, efuse lock bits). */
    rot_set_key_auto_load();
    rot_set_fpk();
    rot_system_general_control();

    /* E7. ROT debug authentication and SWD access control.
     *     SWD is explicitly disabled; the debug-auth flow may re-enable it
     *     later if a valid debug certificate is presented. */
    rot_set_debug_authentication_control();
    rot_swd_control(false);

    /* E8. OTP (One-Time Programmable) read / write protection.
     *
     *     Per-range read protection: iterate over all hardware-defined
     *     protection ranges and configure the read-protect window and lock
     *     according to the corresponding AON OTP flags. */
    for (EFLASH_PROTECTION_RANGE_IDX_TYPE idx = EFLASH_PROTECTION_RANGE_0;
         idx < EFLASH_PROTECTION_RANGE_MAX; idx++)
    {
        uint16_t otp_offset_begin =
            ((uint16_t *)otp_ram->map.otp_protect_addr)[idx * 2];
        uint16_t otp_offset_end   =
            ((uint16_t *)otp_ram->map.otp_protect_addr)[idx * 2 + 1];
        otp_set_read_protect(idx, otp_offset_begin, otp_offset_end,
                             AON_REG_READ_BITFIELD(AON_REG_FW_GENERAL_REG14X,
                                                   IS_ENABLE_OTP_READ_PROTECT));
        otp_set_read_protect_lock(idx,
                                  AON_REG_READ_BITFIELD(AON_REG_FW_GENERAL_REG14X,
                                                        IS_OTP_READ_PROTECT_LOCK));
    }

    /* Write-protect the entire OTP region (range 0, address 0x0 … OTP_TOTAL_SIZE−1). */
    otp_set_write_protect(EFLASH_PROTECTION_RANGE_0,
                          0x0,
                          0x0 + OTP_TOTAL_SIZE - 1,
                          AON_REG_READ_BITFIELD(AON_REG_FW_GENERAL_REG14X,
                                               IS_ENABLE_OTP_WRITE_PROTECT));
    otp_set_write_protect_lock(EFLASH_PROTECTION_RANGE_0,
                               AON_REG_READ_BITFIELD(AON_REG_FW_GENERAL_REG14X,
                                                     IS_OTP_WRITE_PROTECT_LOCK));
}

/**
 * rtl87x2j_platform_late_init - Post-kernel SoC platform initialization.
 *
 * Called from a Zephyr SYS_INIT() APPLICATION hook after the scheduler and
 * memory allocator are fully operational.  Drivers here require OS services
 * (timers, heap) or must register Zephyr interrupt handlers instead of
 * writing bare ROM vectors.
 *
 * See rtl87x2j_platform_init.h for the full step-by-step sequence.
 */
void rtl87x2j_platform_late_init(void)
{
    eflash_assign_func_pointer();

    rtl87x2j_sys_patch_init();
    /*
     * timestamp_init() is intentionally omitted.
     * The TIMESTAMP_IRQ is registered in this function.
     */

    /* E9. FT-OTP: load factory-trim calibration data (voltage, frequency, …)
     *     from OTP into platform registers. */
    ft_otp_init();

    /*
     * Log subsystem init (log_module_trace_init, log_uart_dma_init,
     * log_patch_init, soc_log_module_init) is deferred to the Zephyr
     * application layer and must not run here in the pre-kernel context.
     */

    /* E12. PCK600 power-domain controller init; scheduling-plan table init
     *      (governs which peripherals are active in each power state). */
    pck600_init();
    schedule_plan_init();

    /* E13. SI-flow (slow-interface thermal-calibration) data structures init.
     *      Prepares the data needed by the PMU voltage tuning in E14. */
    si_flow_data_init();

    /* E14. PMU: apply post-trim voltage adjustments derived from OTP factory
     *      calibration data to compensate for process variation. */
    pmu_apply_voltage_tune();

    /* E15. Enable the RAP peripheral bus clock, start the RAP engine, and
     *      initialise the temperature-measurement ADC conversion tables. */
    RCC_ClockCmd(RAP_CLOCK, ENABLE);
    RAP_Cmd(ENABLE);
    TM_ConvertTemperatureInit();

    /* E16. Oscillator calibration: trim RC and crystal oscillators using the
     *      calibration values stored in OTP. */
    clock_osc_cal_init();

    /* E11. Register the RXI300 clock-rate query callback so the bus-fabric
     *      driver can report the current AHB frequency to other subsystems. */
    update_rxi300_get_clk_unit_rate_function(clock_get_unit_rate);

    /* E17. Low-level hardware and CPU setup:
     *        - RAM power-gating (power down unused SRAM banks)
     *        - MPU region configuration
     *        - FPU enable (lazy stacking) */
    hal_setup_hardware();
    hal_setup_cpu();

    /* E18. Install the buffered-log flush callback.  ROM log calls accumulate
     *      in a ring buffer during early boot; once the Zephyr UART driver is
     *      ready, print_buffered_log() drains the buffer to the console. */
    extern void (*buf_output)(void);
    extern void print_buffered_log(void);
    buf_output = print_buffered_log;

    /* E10. Select the active-mode system clock source based on boot config
     *      and OTP calibration data loaded in E9. */
    set_active_mode_clk_src();

    /* L1. Wakeup-source detection and configuration (GPIO, timer, BLE, …). */
    wakeup_init();

    /* L2. Platform power manager: register sleep-entry / wake-exit callbacks
     *     and initialise power-state transition policies. */
    power_manager_init();

    /* L3. Thermal meter: initialize the on-chip temperature sensor peripheral
     *     (ADC + reference circuitry). */
    thermal_meter_init();

    /* L4. RF PHY initialization:
     *     - phy_hw_control_init(): configure the PHY hardware control block.
     *     - phy_init(): bring up the full PHY stack.
     *     Both are called with false = cold boot (not resuming from deep sleep). */
    phy_hw_control_init(false);
    phy_init(false);

    /* L5. Thermal tracking: start the TMETER_FW_IRQn periodic handler which
     *     monitors die temperature and adjusts RF / PMU parameters in real time
     *     to compensate for thermal drift. */
   thermal_tracking_init(); /* also updates TMETER_FW_IRQn vector */

    /* L6. AMU (Analog Measurement Unit): load the ROM-defined measurement
     *     script, then start the continuous background measurement engine. */
    amu_script_init();
    amu_init();

    /* platform_mp_init() is reserved for future multi-processor support
     * and is not yet needed on RTL87x2J single-core configurations. */

    /* L7. Switch the log UART clock to auto-gate mode so it is gated during
     *     light-sleep intervals, reducing idle-mode power consumption. */
    extern void log_uart_switch_clock_auto_mode(bool enable);
    log_uart_switch_clock_auto_mode(true);

    /* L8. Register hardware timer ISR handlers for TIMER0 channels 0 and 1.
     *     Must run after the Zephyr IRQ table is fully set up. */
    extern void TIMER_IRQInit(void);
    TIMER_IRQInit(); /* also updates TIMER0_CH0_CH1_IRQn vector */
}
