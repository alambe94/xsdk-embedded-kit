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

// @file xgpio_drv.c
// @brief CH32H417 General Purpose Input/Output (GPIO) driver implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xgpio_drv.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xGPIO_Init(GPIO_TypeDef *gpiox, uint32_t pin, const xGPIO_Config_t *cfg)
{
    if ((gpiox == NULL) || (cfg == NULL) || (pin >= 16U))
    {
        return;
    }

    uint32_t register_val;

    if (pin < 8U)
    {
        uint32_t shift = pin * 4U;
        register_val = gpiox->CFGLR;
        register_val &= ~(0xFU << shift);
        register_val |= ((uint32_t)cfg->mode) << shift;
        gpiox->CFGLR = register_val;
    }
    else
    {
        uint32_t shift = (pin - 8U) * 4U;
        register_val = gpiox->CFGHR;
        register_val &= ~(0xFU << shift);
        register_val |= ((uint32_t)cfg->mode) << shift;
        gpiox->CFGHR = register_val;
    }

    uint32_t speed_shift = pin * 2U;
    register_val = gpiox->SPEED;
    register_val &= ~(0x3U << speed_shift);
    register_val |= ((uint32_t)cfg->speed) << speed_shift;
    gpiox->SPEED = register_val;
}

void xGPIO_Configure_Pin(GPIO_TypeDef *gpiox, uint32_t pin, uint32_t af_num)
{
    if ((gpiox == NULL) || (pin >= 16U) || (af_num > 15U))
    {
        return;
    }

    uint32_t register_val;

    if (gpiox == GPIOA)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOA_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOA_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOA_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOA_AFHR = register_val;
        }
    }
    else if (gpiox == GPIOB)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOB_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOB_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOB_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOB_AFHR = register_val;
        }
    }
    else if (gpiox == GPIOC)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOC_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOC_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOC_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOC_AFHR = register_val;
        }
    }
    else if (gpiox == GPIOD)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOD_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOD_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOD_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOD_AFHR = register_val;
        }
    }
    else if (gpiox == GPIOE)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOE_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOE_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOE_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOE_AFHR = register_val;
        }
    }
    else if (gpiox == GPIOF)
    {
        if (pin < 8U)
        {
            uint32_t shift = pin * 4U;
            register_val = AFIO->GPIOF_AFLR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOF_AFLR = register_val;
        }
        else
        {
            uint32_t shift = (pin - 8U) * 4U;
            register_val = AFIO->GPIOF_AFHR;
            register_val &= ~(0xFU << shift);
            register_val |= af_num << shift;
            AFIO->GPIOF_AFHR = register_val;
        }
    }
    else
    {
        // Invalid GPIO block
    }
}

void xGPIO_Pin_Write(GPIO_TypeDef *gpiox, uint32_t pin, bool level)
{
    if ((gpiox == NULL) || (pin >= 16U))
    {
        return;
    }

    if (level)
    {
        gpiox->BSHR = 1UL << pin;
    }
    else
    {
        gpiox->BCR = 1UL << pin;
    }
}

void xGPIO_Pin_Toggle(GPIO_TypeDef *gpiox, uint32_t pin)
{
    if ((gpiox == NULL) || (pin >= 16U))
    {
        return;
    }

    uint32_t odr = gpiox->OUTDR;
    if ((odr & (1UL << pin)) != 0U)
    {
        gpiox->BCR = 1UL << pin;
    }
    else
    {
        gpiox->BSHR = 1UL << pin;
    }
}

bool xGPIO_Pin_Read(GPIO_TypeDef *gpiox, uint32_t pin)
{
    if ((gpiox == NULL) || (pin >= 16U))
    {
        return false;
    }

    return (gpiox->INDR & (1UL << pin)) != 0U;
}

// EOF /////////////////////////////////////////////////////////////////////////////
