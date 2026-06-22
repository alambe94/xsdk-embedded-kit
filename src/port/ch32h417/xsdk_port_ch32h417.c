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
#include <stddef.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xsdk_port_ch32h417.h"

// VARIABLES ///////////////////////////////////////////////////////////////////
xGPIO_Context_t g_gpio_a_ctx;
xGPIO_CH32H417_Context_t g_ch32_gpio_a_ctx = {.gpiox = GPIOA};

xGPIO_Context_t g_gpio_b_ctx;
xGPIO_CH32H417_Context_t g_ch32_gpio_b_ctx = {.gpiox = GPIOB};

xGPIO_Context_t g_gpio_c_ctx;
xGPIO_CH32H417_Context_t g_ch32_gpio_c_ctx = {.gpiox = GPIOC};

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

void xSDK_Port_Init(void)
{
    // Enable the AFIO (Alternate Function I/O) subsystem clock
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_AFIO);

    // Initialize HIL GPIO context instances
    xGPIO_Instance_t gpio_a_inst = {.ops = &xGPIO_CH32H417_Driver_Ops, .driver_ctx = &g_ch32_gpio_a_ctx};
    (void)xGPIO_Init(&g_gpio_a_ctx, &gpio_a_inst, NULL);

    xGPIO_Instance_t gpio_b_inst = {.ops = &xGPIO_CH32H417_Driver_Ops, .driver_ctx = &g_ch32_gpio_b_ctx};
    (void)xGPIO_Init(&g_gpio_b_ctx, &gpio_b_inst, NULL);

    xGPIO_Instance_t gpio_c_inst = {.ops = &xGPIO_CH32H417_Driver_Ops, .driver_ctx = &g_ch32_gpio_c_ctx};
    (void)xGPIO_Init(&g_gpio_c_ctx, &gpio_c_inst, NULL);
}

void xSDK_Port_USART1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_USART1);

    xGPIO_Pin_Config_t gpio_cfg = {
        .mode = xGPIO_PIN_MODE_ALTERNATE_PUSH_PULL,
        .speed = xGPIO_PIN_SPEED_VERY_HIGH,
        .pull = xGPIO_PIN_PULL_NONE,
        .alternate_function = 7U};
    (void)xGPIO_Configure_Pin(&g_gpio_a_ctx, 9U, &gpio_cfg); // PA9 AF7 is USART1_TX
}

void xSDK_Port_I2C1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_I2C1);

    xGPIO_Pin_Config_t i2c_gpio = {
        .mode = xGPIO_PIN_MODE_ALTERNATE_OPEN_DRAIN,
        .speed = xGPIO_PIN_SPEED_VERY_HIGH,
        .pull = xGPIO_PIN_PULL_NONE,
        .alternate_function = 4U};
    (void)xGPIO_Configure_Pin(&g_gpio_b_ctx, 6U, &i2c_gpio); // PB6 AF4 is I2C1_SCL
    (void)xGPIO_Configure_Pin(&g_gpio_b_ctx, 7U, &i2c_gpio); // PB7 AF4 is I2C1_SDA
}

void xSDK_Port_SPI1_Pinmux_Init(void)
{
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_SPI1);

    xGPIO_Pin_Config_t spi_gpio = {
        .mode = xGPIO_PIN_MODE_ALTERNATE_PUSH_PULL,
        .speed = xGPIO_PIN_SPEED_VERY_HIGH,
        .pull = xGPIO_PIN_PULL_NONE,
        .alternate_function = 5U};
    (void)xGPIO_Configure_Pin(&g_gpio_a_ctx, 5U, &spi_gpio); // PA5 AF5 is SPI1_SCK
    (void)xGPIO_Configure_Pin(&g_gpio_a_ctx, 6U, &spi_gpio); // PA6 AF5 is SPI1_MISO
    (void)xGPIO_Configure_Pin(&g_gpio_a_ctx, 7U, &spi_gpio); // PA7 AF5 is SPI1_MOSI

    xGPIO_Pin_Config_t cs_gpio = {
        .mode = xGPIO_PIN_MODE_OUTPUT_PUSH_PULL,
        .speed = xGPIO_PIN_SPEED_VERY_HIGH,
        .pull = xGPIO_PIN_PULL_NONE,
        .alternate_function = 0U};
    (void)xGPIO_Configure_Pin(&g_gpio_a_ctx, 4U, &cs_gpio);
    (void)xGPIO_Pin_Write(&g_gpio_a_ctx, 4U, true); // Deselect chip initially (active low)
}
