// Copyright 2022 alambe94
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

// @file xsdk_sbl_board_init.c
// @brief xSDK SBL board initialization for AM243x R5FSS0-0.
//
// Replaces SysConfig-generated files for the SBL build.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/MpuP_armv7.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SystemP.h>
#include <drivers/sciclient.h>
#include <drivers/bootloader.h>
#include <drivers/hw_include/am64x_am243x/cslr_soc_defines.h>

// MODULE INCLUDES
#include "sbl/ti_drivers_config.h"
#include "sbl/ti_drivers_open_close.h"
#include "xsdk_soc_mmr.h"
#include "xuart.h"
#include "xuart_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////////
#define SBL_UART0_BASE           (0x02800000U)
#define TIMER8_CLK_SRC_MUX_ADDR  (0x430081D0U)
#define TIMER8_BASE_ADDR         (0x02480000U)
#define TIMER8_MUX_PARTITION     (2U)
#define CONFIG_MPU_NUM_REGIONS   (5U)
#define BOOT_SECTION             __attribute__((section(".text.boot")))
#define RODATA_CFG               __attribute__((section(".rodata.cfg")))

static xUART_Context_t s_sbl_uart_ctx;
static xUART_AM243x_Context_t s_sbl_am243x_uart_ctx;

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////
// DPL configuration structures (same layout as SysConfig ESK FreeRTOS example)
HwiP_Config gHwiConfig = {
    .intcBaseAddr = 0x2FFF0000U,
};

const CacheP_Config gCacheConfig RODATA_CFG = {
    .enable             = 1U,
    .enableForceWrThru  = 0U,
};

const MpuP_Config gMpuConfig RODATA_CFG = {
    .numRegions             = CONFIG_MPU_NUM_REGIONS,
    .enableBackgroundRegion = 0U,
    .enableMpu              = 1U,
};

const MpuP_RegionConfig gMpuRegionConfig[CONFIG_MPU_NUM_REGIONS] RODATA_CFG = {
    {
        // 0x0 2 GB - device, execute-never (covers all peripherals and unmapped)
        .baseAddr = 0x0U,
        .size = MpuP_RegionSize_2G,
        .attrs = {
            .isEnable = 1U,
            .isCacheable = 0U,
            .isBufferable = 0U,
            .isSharable = 1U,
            .isExecuteNever = 1U,
            .tex = 0U,
            .accessPerm = MpuP_AP_S_RW_U_R,
            .subregionDisableMask = 0U
        },
    },
    {
        // 0x0 32 KB - ATCM, cacheable writeback
        .baseAddr = 0x0U,
        .size = MpuP_RegionSize_32K,
        .attrs = {
            .isEnable = 1U,
            .isCacheable = 1U,
            .isBufferable = 1U,
            .isSharable = 0U,
            .isExecuteNever = 0U,
            .tex = 1U,
            .accessPerm = MpuP_AP_S_RW_U_R,
            .subregionDisableMask = 0U
        },
    },
    {
        // 0x41010000 32 KB - BTCM0, cacheable writeback
        .baseAddr = 0x41010000U,
        .size = MpuP_RegionSize_32K,
        .attrs = {
            .isEnable = 1U,
            .isCacheable = 1U,
            .isBufferable = 1U,
            .isSharable = 0U,
            .isExecuteNever = 0U,
            .tex = 1U,
            .accessPerm = MpuP_AP_S_RW_U_R,
            .subregionDisableMask = 0U
        },
    },
    {
        // 0x70000000 2 MB - MSRAM, cacheable writeback
        .baseAddr = 0x70000000U,
        .size = MpuP_RegionSize_2M,
        .attrs = {
            .isEnable = 1U,
            .isCacheable = 1U,
            .isBufferable = 1U,
            .isSharable = 0U,
            .isExecuteNever = 0U,
            .tex = 1U,
            .accessPerm = MpuP_AP_S_RW_U_R,
            .subregionDisableMask = 0U
        },
    },
    {
        // 0x60000000 256 MB - OSPI flash, read-only cacheable
        .baseAddr = 0x60000000U,
        .size = MpuP_RegionSize_256M,
        .attrs = {
            .isEnable = 1U,
            .isCacheable = 1U,
            .isBufferable = 1U,
            .isSharable = 0U,
            .isExecuteNever = 0U,
            .tex = 1U,
            .accessPerm = MpuP_AP_ALL_R,
            .subregionDisableMask = 0U
        },
    },
};

// Bootloader config (null boot - no image to load, other cores run WFI loop)
Bootloader_Config gBootloaderConfig[] = {
    {
        .fxns                = NULL,
        .args                = NULL,
        .bootMedia           = 0U,
        .isAppimageSigned    = 0U,
        .disableAppImageAuth = 0U,
        .initICSSCores       = 0U,
        .enableScratchMem    = 0U,
    },
};

uint32_t gBootloaderConfigNum = 1U;

ClockP_Config gClockConfig = {
    .timerBaseAddr       = TIMER8_BASE_ADDR,
    .timerHwiIntNum      = 160,
    .timerInputClkHz     = 25000000U,
    .timerInputPreScaler = 1U,
    .usecPerTick         = 1000U,
    .intrPriority        = 15U,
    .isPulseInterrupt    = 0U,
};

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
void putchar_(char c);
void DebugP_uartLogWriterPutChar(char c);
void DebugP_uartSetDrvIndex(uint32_t uart_drv_idx);
void BOOT_SECTION __mpu_init(void);
void System_init(void);
void System_deinit(void);
int32_t Drivers_open(void);
void Drivers_close(void);

__attribute__((constructor, used))
static void xsdk_sbl_dpl_init(void);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Constructor: runs after BSS init, before main().
// Sciclient_init MUST complete before main() calls Bootloader_socWaitForFWBoot.
__attribute__((constructor, used))
static void xsdk_sbl_dpl_init(void)
{
    HwiP_init();

    DebugP_logZoneEnable(DebugP_LOG_ZONE_ERROR);
    DebugP_logZoneEnable(DebugP_LOG_ZONE_WARN);

    // Route TIMER8 to MCU_HFOSC0 (25 MHz) - ClockP_init needs a running timer
    xsdk_soc_mmr_unlock_main(TIMER8_MUX_PARTITION);
    *(volatile uint32_t *)TIMER8_CLK_SRC_MUX_ADDR = 0U;
    xsdk_soc_mmr_lock_main(TIMER8_MUX_PARTITION);
    ClockP_init();

    // R5FSS0-0 core ID = CSL_CORE_ID_R5FSS0_0 = 1
    (void)Sciclient_init(CSL_CORE_ID_R5FSS0_0);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// DebugP UART output - used by DebugP_log internals and putchar_
void putchar_(char c)
{
    (void)xUART_Transmit(&s_sbl_uart_ctx, (const uint8_t *)&c, 1U, 1000U);
}

void DebugP_uartLogWriterPutChar(char c)
{
    (void)xUART_Transmit(&s_sbl_uart_ctx, (const uint8_t *)&c, 1U, 1000U);
}

// Stub - xSDK does not use TI UART driver index mechanism
void DebugP_uartSetDrvIndex(uint32_t uart_drv_idx)
{
    (void)uart_drv_idx;
}

// __mpu_init - called from _c_int00 BEFORE BSS is zeroed.
// Only bare register writes are safe here; no global state reads.
void BOOT_SECTION __mpu_init(void)
{
    MpuP_init();
    CacheP_init();
}

// System_init / System_deinit - called from main() after board config
void System_init(void)
{
    HwiP_enable();
}

void System_deinit(void)
{
    HwiP_disable();
}

// Drivers_open / Drivers_close - called from main()
// Sciclient is already initialised in the constructor above.
int32_t Drivers_open(void)
{
    s_sbl_am243x_uart_ctx.base_addr = SBL_UART0_BASE;
    s_sbl_am243x_uart_ctx.input_clock_hz = 0U;

    xUART_Config_t config = {
        .baud_rate = 0U,
        .data_bits = xUART_DATA_BITS_8,
        .stop_bits = xUART_STOP_BITS_1,
        .parity = xUART_PARITY_NONE,
        .flow_control = xUART_FLOW_CONTROL_NONE
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

// EOF /////////////////////////////////////////////////////////////////////////////
