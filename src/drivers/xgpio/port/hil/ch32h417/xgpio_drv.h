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

// @file xgpio_drv.h
// @brief CH32H417 HIL GPIO driver header.
//

#ifndef XGPIO_DRV_H
#define XGPIO_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xgpio.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        GPIO_TypeDef *gpiox;
    } xGPIO_CH32H417_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////
    extern const xGPIO_Driver_Ops_t xGPIO_CH32H417_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
