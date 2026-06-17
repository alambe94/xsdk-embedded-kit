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

// @file xrcc_drv.c
// @brief CH32H417 Reset and Clock Control (RCC) driver implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xrcc_drv.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xRCC_Enable_Periph_Clock(xRCC_Periph_t periph)
{
    switch (periph)
    {
    case xRCC_PERIPH_AFIO:
        RCC->HB2PCENR |= RCC_AFIOEN;
        break;
    case xRCC_PERIPH_GPIOA:
        RCC->HB2PCENR |= RCC_IOPAEN;
        break;
    case xRCC_PERIPH_GPIOB:
        RCC->HB2PCENR |= RCC_IOPBEN;
        break;
    case xRCC_PERIPH_GPIOC:
        RCC->HB2PCENR |= RCC_IOPCEN;
        break;
    case xRCC_PERIPH_GPIOD:
        RCC->HB2PCENR |= RCC_IOPDEN;
        break;
    case xRCC_PERIPH_GPIOE:
        RCC->HB2PCENR |= RCC_IOPEEN;
        break;
    case xRCC_PERIPH_GPIOF:
        RCC->HB2PCENR |= RCC_IOPFEN;
        break;
    case xRCC_PERIPH_USART1:
        RCC->HB2PCENR |= RCC_USART1EN;
        break;
    case xRCC_PERIPH_USART2:
        RCC->HB1PCENR |= RCC_USART2EN;
        break;
    case xRCC_PERIPH_USART3:
        RCC->HB1PCENR |= RCC_USART3EN;
        break;
    case xRCC_PERIPH_I2C1:
        RCC->HB1PCENR |= RCC_I2C1EN;
        break;
    case xRCC_PERIPH_I2C2:
        RCC->HB1PCENR |= RCC_I2C2EN;
        break;
    case xRCC_PERIPH_SPI1:
        RCC->HB2PCENR |= RCC_SPI1EN;
        break;
    case xRCC_PERIPH_SPI2:
        RCC->HB1PCENR |= RCC_SPI2EN;
        break;
    case xRCC_PERIPH_TIM1:
        RCC->HB2PCENR |= RCC_TIM1EN;
        break;
    case xRCC_PERIPH_TIM2:
        RCC->HB1PCENR |= RCC_TIM2EN;
        break;
    case xRCC_PERIPH_TIM3:
        RCC->HB1PCENR |= RCC_TIM3EN;
        break;
    case xRCC_PERIPH_TIM4:
        RCC->HB1PCENR |= RCC_TIM4EN;
        break;
    default:
        break;
    }
}

void xRCC_Disable_Periph_Clock(xRCC_Periph_t periph)
{
    switch (periph)
    {
    case xRCC_PERIPH_AFIO:
        RCC->HB2PCENR &= ~RCC_AFIOEN;
        break;
    case xRCC_PERIPH_GPIOA:
        RCC->HB2PCENR &= ~RCC_IOPAEN;
        break;
    case xRCC_PERIPH_GPIOB:
        RCC->HB2PCENR &= ~RCC_IOPBEN;
        break;
    case xRCC_PERIPH_GPIOC:
        RCC->HB2PCENR &= ~RCC_IOPCEN;
        break;
    case xRCC_PERIPH_GPIOD:
        RCC->HB2PCENR &= ~RCC_IOPDEN;
        break;
    case xRCC_PERIPH_GPIOE:
        RCC->HB2PCENR &= ~RCC_IOPEEN;
        break;
    case xRCC_PERIPH_GPIOF:
        RCC->HB2PCENR &= ~RCC_IOPFEN;
        break;
    case xRCC_PERIPH_USART1:
        RCC->HB2PCENR &= ~RCC_USART1EN;
        break;
    case xRCC_PERIPH_USART2:
        RCC->HB1PCENR &= ~RCC_USART2EN;
        break;
    case xRCC_PERIPH_USART3:
        RCC->HB1PCENR &= ~RCC_USART3EN;
        break;
    case xRCC_PERIPH_I2C1:
        RCC->HB1PCENR &= ~RCC_I2C1EN;
        break;
    case xRCC_PERIPH_I2C2:
        RCC->HB1PCENR &= ~RCC_I2C2EN;
        break;
    case xRCC_PERIPH_SPI1:
        RCC->HB2PCENR &= ~RCC_SPI1EN;
        break;
    case xRCC_PERIPH_SPI2:
        RCC->HB1PCENR &= ~RCC_SPI2EN;
        break;
    case xRCC_PERIPH_TIM1:
        RCC->HB2PCENR &= ~RCC_TIM1EN;
        break;
    case xRCC_PERIPH_TIM2:
        RCC->HB1PCENR &= ~RCC_TIM2EN;
        break;
    case xRCC_PERIPH_TIM3:
        RCC->HB1PCENR &= ~RCC_TIM3EN;
        break;
    case xRCC_PERIPH_TIM4:
        RCC->HB1PCENR &= ~RCC_TIM4EN;
        break;
    default:
        break;
    }
}

uint32_t xRCC_Get_HCLK_Freq(void)
{
    uint32_t sws = RCC->CFGR0 & 0x0000000CU;
    uint32_t sysclk = 25000000U;

    if (sws == 0x00U)
    {
        sysclk = 25000000U;
    }
    else if (sws == 0x04U)
    {
        sysclk = 25000000U;
    }
    else if (sws == 0x08U)
    {
        uint32_t syspll_sel = RCC->PLLCFGR & 0x70000000U;
        if (syspll_sel == 0x00000000U)
        {
            uint32_t pllmull = RCC->PLLCFGR & 0x0000001FU;
            uint32_t pllsource = RCC->PLLCFGR & 0x000000E0U;
            uint32_t presc = ((RCC->PLLCFGR & 0x00003F00U) >> 8) + 1U;
            uint32_t tmp1 = 25000000U;
            const uint32_t serdes_mul_table[16] = {25, 28, 30, 32, 35, 38, 40, 45, 50, 56, 60, 64, 70, 76, 80, 90};

            if (pllsource == 0xA0U)
            {
                tmp1 = 500000000U / presc;
            }
            else if (pllsource == 0xE0U)
            {
                uint32_t serdes_mul_idx = (RCC->PLLCFGR2 >> 16) & 0x0FU;
                uint32_t serdes_mul = serdes_mul_table[serdes_mul_idx];
                tmp1 = (25000000U * serdes_mul) / (2U * presc);
            }
            else if (pllsource == 0x80U)
            {
                tmp1 = 480000000U / presc;
            }
            else if (pllsource == 0xC0U)
            {
                tmp1 = 125000000U / presc;
            }
            else if (pllsource == 0x20U)
            {
                tmp1 = 25000000U / presc;
            }
            else
            {
                tmp1 = 25000000U / presc;
            }

            const uint32_t pll_mul_table[32] = {4,  6,  7,  8,  17, 9,  19, 10, 21, 11, 23, 12, 25, 13, 14, 15,
                                                16, 17, 18, 19, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 59};
            uint32_t pll_mul = pll_mul_table[pllmull];

            if ((pllmull == 4U) || (pllmull == 6U) || (pllmull == 8U) || (pllmull == 10U) || (pllmull == 12U))
            {
                sysclk = (tmp1 * pll_mul) >> 1U;
            }
            else
            {
                sysclk = tmp1 * pll_mul;
            }
        }
        else if (syspll_sel == 0x40000000U)
        {
            sysclk = 480000000U;
        }
        else if (syspll_sel == 0x50000000U)
        {
            sysclk = 500000000U;
        }
        else if (syspll_sel == 0x60000000U)
        {
            uint32_t serdes_mul_idx = (RCC->PLLCFGR2 >> 16) & 0x0FU;
            const uint32_t serdes_mul_table[16] = {25, 28, 30, 32, 35, 38, 40, 45, 50, 56, 60, 64, 70, 76, 80, 90};
            uint32_t serdes_mul = serdes_mul_table[serdes_mul_idx];
            sysclk = (25000000U * serdes_mul) / 2U;
        }
        else if (syspll_sel == 0x70000000U)
        {
            sysclk = 125000000U;
        }
    }

    uint32_t hpre = (RCC->CFGR0 & 0x000000F0U) >> 4U;
    const uint32_t hb_presc_table[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
    uint32_t sysclk_div = sysclk >> hb_presc_table[hpre];

    uint32_t fpre = (RCC->CFGR0 & 0x00030000U) >> 16U;
    const uint32_t fpre_table[4] = {0, 1, 2, 2};
    uint32_t hclk = sysclk_div >> fpre_table[fpre];

    return hclk;
}

// EOF /////////////////////////////////////////////////////////////////////////////
