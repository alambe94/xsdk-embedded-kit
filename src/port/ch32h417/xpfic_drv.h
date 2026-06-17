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

// @file xpfic_drv.h
// @brief CH32H417 Programmable Fast Interrupt Controller (PFIC) wrapper driver.
//

#ifndef XPFIC_DRV_H
#define XPFIC_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xPFIC_Enable_IRQ(IRQn_Type irq);
    void xPFIC_Disable_IRQ(IRQn_Type irq);
    void xPFIC_Set_Priority(IRQn_Type irq, uint8_t priority);
    void xPFIC_Clear_Pending(IRQn_Type irq);
    void xPFIC_Set_Pending(IRQn_Type irq);
    bool xPFIC_Get_Pending(IRQn_Type irq);
    bool xPFIC_Get_Active(IRQn_Type irq);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XPFIC_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
