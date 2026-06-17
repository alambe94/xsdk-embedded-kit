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

// @file xsdk_port_ch32h417.c
// @brief CH32H417 SoC port integration and initialization implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xsdk_port_ch32h417.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

void xSDK_Port_Init(void)
{
    // Enable the AFIO (Alternate Function I/O) subsystem clock
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_AFIO);
}

void xSDK_Port_USART1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_GPIOA);
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_USART1);

    xGPIO_Config_t gpio_cfg = {
        .mode  = xGPIO_MODE_AF_PP,
        .speed = xGPIO_SPEED_VERY_HIGH
    };
    xGPIO_Init(GPIOA, 9U, &gpio_cfg);
    xGPIO_Configure_Pin(GPIOA, 9U, 7U); // PA9 AF7 is USART1_TX
}

void xSDK_Port_I2C1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_GPIOB);
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_I2C1);

    xGPIO_Config_t i2c_gpio = {
        .mode  = xGPIO_MODE_AF_OD,
        .speed = xGPIO_SPEED_VERY_HIGH
    };
    xGPIO_Init(GPIOB, 6U, &i2c_gpio);
    xGPIO_Init(GPIOB, 7U, &i2c_gpio);
    xGPIO_Configure_Pin(GPIOB, 6U, 4U); // PB6 AF4 is I2C1_SCL
    xGPIO_Configure_Pin(GPIOB, 7U, 4U); // PB7 AF4 is I2C1_SDA
}

void xSDK_Port_SPI1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_GPIOA);
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_SPI1);

    xGPIO_Config_t spi_gpio = {
        .mode  = xGPIO_MODE_AF_PP,
        .speed = xGPIO_SPEED_VERY_HIGH
    };
    xGPIO_Init(GPIOA, 5U, &spi_gpio);
    xGPIO_Init(GPIOA, 6U, &spi_gpio);
    xGPIO_Init(GPIOA, 7U, &spi_gpio);
    xGPIO_Configure_Pin(GPIOA, 5U, 5U); // PA5 AF5 is SPI1_SCK
    xGPIO_Configure_Pin(GPIOA, 6U, 5U); // PA6 AF5 is SPI1_MISO
    xGPIO_Configure_Pin(GPIOA, 7U, 5U); // PA7 AF5 is SPI1_MOSI

    xGPIO_Config_t cs_gpio = {
        .mode  = xGPIO_MODE_OUTPUT_PP,
        .speed = xGPIO_SPEED_VERY_HIGH
    };
    xGPIO_Init(GPIOA, 4U, &cs_gpio);
    xGPIO_Pin_Write(GPIOA, 4U, true); // Deselect chip initially (active low)
}

// EOF /////////////////////////////////////////////////////////////////////////////
