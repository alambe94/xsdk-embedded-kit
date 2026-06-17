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
// @brief CH32H417 Timer hardware port implementation for the xTIMER driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xtimer.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xTIMER_Init_Periodic(uint32_t base_addr, uint32_t period_us, uint32_t module_clk_hz)
{
    TIM_TypeDef *tim = (TIM_TypeDef *)base_addr;

    // 1. Enable peripheral clock and reset timer block dynamically
    if (base_addr == TIM1_BASE)
    {
        RCC->HB2PCENR |= RCC_TIM1EN;
        RCC->HB2PRSTR |= RCC_TIM1RST;
        RCC->HB2PRSTR &= ~RCC_TIM1RST;
    }
    else if (base_addr == TIM2_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM2EN;
        RCC->HB1PRSTR |= RCC_TIM2RST;
        RCC->HB1PRSTR &= ~RCC_TIM2RST;
    }
    else if (base_addr == TIM3_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM3EN;
        RCC->HB1PRSTR |= RCC_TIM3RST;
        RCC->HB1PRSTR &= ~RCC_TIM3RST;
    }
    else if (base_addr == TIM4_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM4EN;
        RCC->HB1PRSTR |= RCC_TIM4RST;
        RCC->HB1PRSTR &= ~RCC_TIM4RST;
    }

    // Disable timer count
    tim->CTLR1 = 0U;
    tim->CTLR2 = 0U;

    // 2. Compute prescaler and auto-reload registers
    uint32_t psc = module_clk_hz / 1000000U;
    uint32_t arr = period_us;
    while (arr > 65535U)
    {
        psc *= 2U;
        arr /= 2U;
    }
    if (psc > 0U)
    {
        psc--;
    }
    if (arr > 0U)
    {
        arr--;
    }

    tim->PSC   = (uint16_t)psc;
    tim->ATRLR = (uint16_t)arr;

    // Clear update event flag and configure UIE interrupt
    tim->INTFR      = ~(uint16_t)TIM_UIF;
    tim->DMAINTENR  = TIM_UIE;

    // Seed the counter registers immediately
    tim->SWEVGR = TIM_UG;
}

void xTIMER_Start(uint32_t base_addr)
{
    TIM_TypeDef *tim = (TIM_TypeDef *)base_addr;
    tim->CTLR1 |= TIM_CEN;
}

void xTIMER_Stop(uint32_t base_addr)
{
    TIM_TypeDef *tim = (TIM_TypeDef *)base_addr;
    tim->CTLR1 &= ~(uint16_t)TIM_CEN;
}

void xTIMER_Clear_IRQ(uint32_t base_addr)
{
    TIM_TypeDef *tim = (TIM_TypeDef *)base_addr;
    tim->INTFR = ~(uint16_t)TIM_UIF;
}

// EOF /////////////////////////////////////////////////////////////////////////////
