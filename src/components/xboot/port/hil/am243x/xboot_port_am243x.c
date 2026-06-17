// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xboot_port_am243x.c
// @brief AM243x target-specific port operations and initialization.
//

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/MpuP_armv7.h>
#include <kernel/dpl/SystemP.h>
#include <drivers/bootloader.h>
#include <drivers/sciclient.h>
#include <drivers/hw_include/am64x_am243x/cslr_soc_defines.h>

// MODULE INCLUDES
#include "xboot_port_am243x.h"
#include "xboot_core.h"
#include "xboot_handoff.h"
#include "xboot_return.h"
#include "xsdk_soc_mmr.h"
#include "xuart.h"
#include "xuart_drv.h"
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define SBL_UART0_BASE           (0x02800000U)
#define TIMER8_BASE_ADDR         (0x02480000U)
#define TIMER8_CLK_SRC_MUX_ADDR  (0x430081D0U)
#define TIMER8_MUX_PARTITION     (2U)
#define CONFIG_MPU_NUM_REGIONS   (5U)
#define RODATA_CFG               __attribute__((section(".rodata.cfg")))
#define BOOT_SECTION             __attribute__((section(".text.boot")))

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////
HwiP_Config gHwiConfig = {
    .intcBaseAddr = 0x2FFF0000U,
};

const CacheP_Config gCacheConfig RODATA_CFG = {
    .enable             = 1,
    .enableForceWrThru  = 0,
};

const MpuP_Config gMpuConfig RODATA_CFG = {
    .numRegions            = CONFIG_MPU_NUM_REGIONS,
    .enableBackgroundRegion = 0,
    .enableMpu             = 1,
};

const MpuP_RegionConfig gMpuRegionConfig[CONFIG_MPU_NUM_REGIONS] RODATA_CFG = {
    {   // 0x0 2 GB - device, execute-never
        .baseAddr = 0x0U, .size = MpuP_RegionSize_2G,
        .attrs = { .isEnable = 1, .isCacheable = 0, .isBufferable = 0,
                   .isSharable = 1, .isExecuteNever = 1, .tex = 0,
                   .accessPerm = MpuP_AP_S_RW_U_R, .subregionDisableMask = 0U },
    },
    {   // 0x0 32 KB - ATCM, cacheable writeback
        .baseAddr = 0x0U, .size = MpuP_RegionSize_32K,
        .attrs = { .isEnable = 1, .isCacheable = 1, .isBufferable = 1,
                   .isSharable = 0, .isExecuteNever = 0, .tex = 1,
                   .accessPerm = MpuP_AP_S_RW_U_R, .subregionDisableMask = 0U },
    },
    {   // 0x41010000 32 KB - BTCM0, cacheable writeback
        .baseAddr = 0x41010000U, .size = MpuP_RegionSize_32K,
        .attrs = { .isEnable = 1, .isCacheable = 1, .isBufferable = 1,
                   .isSharable = 0, .isExecuteNever = 0, .tex = 1,
                   .accessPerm = MpuP_AP_S_RW_U_R, .subregionDisableMask = 0U },
    },
    {   // 0x70000000 2 MB - MSRAM, cacheable writeback
        .baseAddr = 0x70000000U, .size = MpuP_RegionSize_2M,
        .attrs = { .isEnable = 1, .isCacheable = 1, .isBufferable = 1,
                   .isSharable = 0, .isExecuteNever = 0, .tex = 1,
                   .accessPerm = MpuP_AP_S_RW_U_R, .subregionDisableMask = 0U },
    },
    {   // 0x60000000 256 MB - OSPI flash, read-only cacheable
        .baseAddr = 0x60000000U, .size = MpuP_RegionSize_256M,
        .attrs = { .isEnable = 1, .isCacheable = 1, .isBufferable = 1,
                   .isSharable = 0, .isExecuteNever = 0, .tex = 1,
                   .accessPerm = MpuP_AP_ALL_R, .subregionDisableMask = 0U },
    },
};

Bootloader_Config gBootloaderConfig[] = {
    {
        .fxns               = NULL,
        .args               = NULL,
        .bootMedia          = 0U,
        .isAppimageSigned   = 0U,
        .disableAppImageAuth = 0U,
        .initICSSCores      = 0U,
        .enableScratchMem   = 0U,
    },
};
uint32_t gBootloaderConfigNum = 1U;

ClockP_Config gClockConfig = {
    .timerBaseAddr       = TIMER8_BASE_ADDR,
    .timerHwiIntNum      = 160,
    .timerInputClkHz     = 25000000,
    .timerInputPreScaler = 1,
    .usecPerTick         = 1000,
    .intrPriority        = 15,
    .isPulseInterrupt    = 0,
};

static xBOOT_Context_t s_boot_context;
static xUART_Context_t s_sbl_uart_ctx;
static xUART_AM243x_Context_t s_sbl_am243x_uart_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
void xBOOT_Main(void);
void __mpu_init(void) BOOT_SECTION;
void putchar_(char c);
void DebugP_uartLogWriterPutChar(char c);
void DebugP_uartSetDrvIndex(uint32_t uartDrvIndex);

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS ////////////////////////////////////////////////////////////

/**
 * @brief __mpu_init - called from startup assembly before BSS initialization.
 */
void BOOT_SECTION __mpu_init(void)
{
    MpuP_init();
    CacheP_init();
}

/**
 * @brief Constructor function running after BSS init and before main.
 */
__attribute__((constructor, used))
static void xsdk_sbl_dpl_init(void)
{
    HwiP_init();

    DebugP_logZoneEnable(DebugP_LOG_ZONE_ERROR);
    DebugP_logZoneEnable(DebugP_LOG_ZONE_WARN);

    // Route TIMER8 to MCU_HFOSC0 (25 MHz)
    xsdk_soc_mmr_unlock_main(TIMER8_MUX_PARTITION);
    *(volatile uint32_t *)TIMER8_CLK_SRC_MUX_ADDR = 0U;
    xsdk_soc_mmr_lock_main(TIMER8_MUX_PARTITION);
    ClockP_init();

    // R5FSS0-0 Core ID = 1
    (void)Sciclient_init(CSL_CORE_ID_R5FSS0_0);
}

void System_init(void)
{
    HwiP_enable();
}

void System_deinit(void)
{
    HwiP_disable();
}

int32_t Drivers_open(void)
{
    s_sbl_am243x_uart_ctx.base_addr = SBL_UART0_BASE;
    s_sbl_am243x_uart_ctx.input_clock_hz = 0U;

    xUART_Config_t config = {
        .baud_rate = 0U,
        .data_bits = xUART_DATA_BITS_8,
        .stop_bits = xUART_STOP_BITS_1,
        .parity = xUART_PARITY_NONE,
        .flow_control = xUART_FLOW_CONTROL_NONE,
        .callbacks.on_event = NULL
    };

    if (xUART_Init(&s_sbl_uart_ctx, &config) != xRETURN_OK)
    {
        return SystemP_FAILURE;
    }

    xUART_Start_Config_t start_cfg = {
        .port = 0U,
        .drv_ops = &xUART_AM243x_Driver_Ops,
        .drv_ctx = &s_sbl_am243x_uart_ctx
    };

    if (xUART_Start(&s_sbl_uart_ctx, &start_cfg) != xRETURN_OK)
    {
        return SystemP_FAILURE;
    }

    return SystemP_SUCCESS;
}

void Drivers_close(void)
{
    (void)xUART_Stop(&s_sbl_uart_ctx);
    (void)xUART_Deinit(&s_sbl_uart_ctx);
    (void)Sciclient_deinit();
}

void putchar_(char c)
{
    (void)xUART_Transmit(&s_sbl_uart_ctx, (const uint8_t *)&c, 1U, 1000U);
}

void DebugP_uartLogWriterPutChar(char c)
{
    (void)xUART_Transmit(&s_sbl_uart_ctx, (const uint8_t *)&c, 1U, 1000U);
}

void DebugP_uartSetDrvIndex(uint32_t uartDrvIndex)
{
    (void)uartDrvIndex;
}

static xRETURN_t am243x_prepare_handoff(void *port_ctx, uint32_t entry_address)
{
    (void)port_ctx;
    (void)entry_address;
    // Disable interrupts globally
    __asm__ volatile ("cpsid iaf");
    return xRETURN_xBOOT_OK;
}

static void am243x_jump(void *port_ctx, uint32_t entry_address)
{
    (void)port_ctx;
    (void)entry_address;

    // Cache invalidation & clean
    CacheP_wbInv((void *)0x70040000, 0xC0000, CacheP_TYPE_ALL); // MSRAM range
    CacheP_wbInv((void *)0x00000000, 0x8000, CacheP_TYPE_ALL);  // ATCM range
    __asm__ volatile ("mcr p15, 0, %0, c7, c5, 0" :: "r"(0)); // I-cache

    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");

    // Invoke TI Bootloader API to reset self cluster and boot application
    Bootloader_Params bootParams;
    Bootloader_BootImageInfo bootImageInfo;
    Bootloader_Handle bootHandle;

    Bootloader_Params_init(&bootParams);
    Bootloader_BootImageInfo_init(&bootImageInfo);

    bootHandle = Bootloader_open(CONFIG_BOOTLOADER0, &bootParams);
    if (bootHandle != NULL)
    {
        (void)Bootloader_bootSelfCpu(bootHandle, &bootImageInfo);
        Bootloader_close(bootHandle);
    }

    // Spin forever if it returns
    for (;;)
    {
    }
}

static void am243x_reset(void *port_ctx)
{
    (void)port_ctx;
    (void)Sciclient_pmDeviceReset(SystemP_WAIT_FOREVER);
}

static const xBOOT_Port_Ops_t s_am243x_port_ops = {
    .prepare_handoff = am243x_prepare_handoff,
    .jump = am243x_jump,
    .reset = am243x_reset
};

const xBOOT_Port_Ops_t *xBOOT_Port_AM243x_Get_Ops(void)
{
    return &s_am243x_port_ops;
}

xRETURN_t xBOOT_Port_AM243x_Init(void)
{
    int32_t status;

    Bootloader_socWaitForFWBoot();

    if (!Bootloader_socIsMCUResetIsoEnabled())
    {
        Sciclient_BoardCfgPrms_t boardCfgPrms_pm = {
            .boardConfigLow = 0U,
            .boardConfigHigh = 0U,
            .boardConfigSize = 0U,
            .devGrp = DEVGRP_ALL,
        };
        status = Sciclient_boardCfgPm(&boardCfgPrms_pm);
        if (status != SystemP_SUCCESS)
        {
            return xRETURN_xERR_xBOOT_INVALID_STATE;
        }

        Sciclient_BoardCfgPrms_t boardCfgPrms_rm = {
            .boardConfigLow = 0U,
            .boardConfigHigh = 0U,
            .boardConfigSize = 0U,
            .devGrp = DEVGRP_ALL,
        };
        status = Sciclient_boardCfgRm(&boardCfgPrms_rm);
        if (status != SystemP_SUCCESS)
        {
            return xRETURN_xERR_xBOOT_INVALID_STATE;
        }

        Bootloader_enableMCUPLL();
    }

    System_init();
    Bootloader_socOpenFirewalls();
    Bootloader_socNotifyFirewallOpen();

    if (Drivers_open() != SystemP_SUCCESS)
    {
        return xRETURN_xERR_xBOOT_INVALID_STATE;
    }

    return xRETURN_xBOOT_OK;
}

/**
 * @brief xBOOT hardware entry point called from startup assembly.
 */
void xBOOT_Main(void)
{
    xRETURN_t status;

    status = xBOOT_Port_AM243x_Init();
    if (status != xRETURN_xBOOT_OK)
    {
        // Init failed, trigger Sciclient device reset
        (void)Sciclient_pmDeviceReset(SystemP_WAIT_FOREVER);
    }

    DebugP_log("\r\n");
    DebugP_log("Starting xBOOT Target Skeleton ... \r\n");

    xBOOT_Config_t config = {
        .storage_ops = NULL, // Staged, read-only OSPI is added in Phase 10
        .storage_ctx = NULL,
        .port_ops = xBOOT_Port_AM243x_Get_Ops(),
        .port_ctx = NULL,
        .force_recovery = false
    };

    status = xBOOT_Init(&s_boot_context, &config);
    if (status == xRETURN_xBOOT_OK)
    {
        (void)xBOOT_Run(&s_boot_context);
    }

    // If boot fails, reset system
    (void)Sciclient_pmDeviceReset(SystemP_WAIT_FOREVER);
}
// EOF /////////////////////////////////////////////////////////////////////////////
