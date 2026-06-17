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

// @file xsystick_drv.h
// @brief CH32H417 Core SysTick timer driver.
//

#ifndef XSYSTICK_DRV_H
#define XSYSTICK_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////
#define xSYSTICK_CTLR_STE   (1UL << 0) /* SysTick Enable */
#define xSYSTICK_CTLR_STIE  (1UL << 1) /* SysTick Interrupt Enable */
#define xSYSTICK_CTLR_STCLK (1UL << 2) /* Clock Source (0 = HCLK/8, 1 = HCLK) */
#define xSYSTICK_CTLR_STRE  (1UL << 3) /* SysTick Auto-Reload Enable */

#define xSYSTICK_CTLR_RUN (xSYSTICK_CTLR_STE | xSYSTICK_CTLR_STIE | xSYSTICK_CTLR_STCLK | xSYSTICK_CTLR_STRE)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void     xSysTick_Init(SysTick_Type *systick, uint32_t compare_value);
    void     xSysTick_Enable(SysTick_Type *systick);
    void     xSysTick_Disable(SysTick_Type *systick);
    uint32_t xSysTick_Get_Counter(SysTick_Type *systick);
    void     xSysTick_Clear_Pending(SysTick_Type *systick);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSYSTICK_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
