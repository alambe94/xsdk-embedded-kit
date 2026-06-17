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
// @brief CH32H417 General Purpose Input/Output (GPIO) driver.
//

#ifndef XGPIO_DRV_H
#define XGPIO_DRV_H

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

    typedef enum
    {
        xGPIO_MODE_INPUT = 0x0U,
        xGPIO_MODE_OUTPUT_PP = 0x1U,
        xGPIO_MODE_OUTPUT_OD = 0x2U,
        xGPIO_MODE_AF_PP = 0x9U,
        xGPIO_MODE_AF_OD = 0xAU,
    } xGPIO_Mode_t;

    typedef enum
    {
        xGPIO_SPEED_LOW = 0x0U,
        xGPIO_SPEED_MEDIUM = 0x1U,
        xGPIO_SPEED_HIGH = 0x2U,
        xGPIO_SPEED_VERY_HIGH = 0x3U,
    } xGPIO_Speed_t;

    typedef struct
    {
        xGPIO_Mode_t mode;
        xGPIO_Speed_t speed;
    } xGPIO_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xGPIO_Init(GPIO_TypeDef *gpiox, uint32_t pin, const xGPIO_Config_t *cfg);
    void xGPIO_Configure_Pin(GPIO_TypeDef *gpiox, uint32_t pin, uint32_t af_num);
    void xGPIO_Pin_Write(GPIO_TypeDef *gpiox, uint32_t pin, bool level);
    void xGPIO_Pin_Toggle(GPIO_TypeDef *gpiox, uint32_t pin);
    bool xGPIO_Pin_Read(GPIO_TypeDef *gpiox, uint32_t pin);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
