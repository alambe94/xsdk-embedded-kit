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

// @file xsystick_drv.c
// @brief CH32H417 Core SysTick timer driver implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xsystick_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

void xSysTick_Init(SysTick_Type *systick, uint32_t compare_value)
{
    if (systick == NULL)
    {
        return;
    }

    systick->CTLR = 0U;
    systick->CNT  = 0U;
    systick->CMP  = compare_value;
}

void xSysTick_Enable(SysTick_Type *systick)
{
    if (systick == NULL)
    {
        return;
    }

    systick->CTLR |= xSYSTICK_CTLR_RUN;
}

void xSysTick_Disable(SysTick_Type *systick)
{
    if (systick == NULL)
    {
        return;
    }

    systick->CTLR &= ~xSYSTICK_CTLR_RUN;
}

uint32_t xSysTick_Get_Counter(SysTick_Type *systick)
{
    if (systick == NULL)
    {
        return 0U;
    }

    return systick->CNT;
}

void xSysTick_Clear_Pending(SysTick_Type *systick)
{
    if (systick == NULL)
    {
        return;
    }

    if (systick == SysTick0)
    {
        SysTick0->ISR &= ~1U;
    }
    else if (systick == SysTick1)
    {
        SysTick0->ISR &= ~2U;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
