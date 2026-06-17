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

// @file xtimer_drv.c
// @brief TI AM243x DMTimer hardware port implementation for the xTIMER driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer.h"

// MACROS //////////////////////////////////////////////////////////////////////////
/* DMTimer register offsets (Keystone3 / AM64x / AM243x) */
#define TIOCP_CFG 0x10U
#define TISR      0x28U
#define TIER      0x2CU
#define TCLR      0x38U
#define TCRR      0x3CU
#define TLDR      0x40U

#define TCLR_ST  (1U << 0)
#define TCLR_AR  (1U << 1)
#define TIER_OVF (1U << 1)
#define TISR_OVF (1U << 1)

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xTIMER_Init_Periodic(uint32_t base_addr, uint32_t period_us, uint32_t module_clk_hz)
{
    uint32_t ticks = (uint32_t)((uint64_t)module_clk_hz * period_us / 1000000U);
    uint32_t load  = 0xFFFFFFFFU - ticks + 1U;

    REG32(base_addr, TIOCP_CFG) = 0x1U;
    while ((REG32(base_addr, TIOCP_CFG) & 0x1U) != 0U)
    {
    }

    REG32(base_addr, TCLR) = 0U;
    REG32(base_addr, TLDR) = load;
    REG32(base_addr, TCRR) = load;
    REG32(base_addr, TISR) = TISR_OVF;
    REG32(base_addr, TIER) = TIER_OVF;
}

void xTIMER_Start(uint32_t base_addr)
{
    REG32(base_addr, TCLR) = TCLR_ST | TCLR_AR;
}

void xTIMER_Stop(uint32_t base_addr)
{
    REG32(base_addr, TCLR) &= ~(uint32_t)TCLR_ST;
}

void xTIMER_Clear_IRQ(uint32_t base_addr)
{
    REG32(base_addr, TISR) = TISR_OVF;
}

// EOF /////////////////////////////////////////////////////////////////////////////
