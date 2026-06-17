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

// @file xrcc_drv.h
// @brief CH32H417 Reset and Clock Control (RCC) driver.
//

#ifndef XRCC_DRV_H
#define XRCC_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        xRCC_PERIPH_AFIO,
        xRCC_PERIPH_GPIOA,
        xRCC_PERIPH_GPIOB,
        xRCC_PERIPH_GPIOC,
        xRCC_PERIPH_GPIOD,
        xRCC_PERIPH_GPIOE,
        xRCC_PERIPH_GPIOF,
        xRCC_PERIPH_USART1,
        xRCC_PERIPH_USART2,
        xRCC_PERIPH_USART3,
        xRCC_PERIPH_I2C1,
        xRCC_PERIPH_I2C2,
        xRCC_PERIPH_SPI1,
        xRCC_PERIPH_SPI2,
        xRCC_PERIPH_TIM1,
        xRCC_PERIPH_TIM2,
        xRCC_PERIPH_TIM3,
        xRCC_PERIPH_TIM4,
    } xRCC_Periph_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xRCC_Enable_Periph_Clock(xRCC_Periph_t periph);
    void xRCC_Disable_Periph_Clock(xRCC_Periph_t periph);
    uint32_t xRCC_Get_HCLK_Freq(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRCC_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
