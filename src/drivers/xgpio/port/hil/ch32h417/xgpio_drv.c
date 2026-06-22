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
// @brief CH32H417 HIL GPIO driver implementation.
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
#include "xrcc_drv.h"

// DEBUG

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t ch32_init(void *driver_ctx, const xGPIO_Config_t *config);
static xRETURN_t ch32_deinit(void *driver_ctx);
static xRETURN_t ch32_configure_pin(void *driver_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config);
static xRETURN_t ch32_pin_write(void *driver_ctx, uint32_t pin, bool level);
static xRETURN_t ch32_pin_read(void *driver_ctx, uint32_t pin, bool *level);
static xRETURN_t ch32_pin_toggle(void *driver_ctx, uint32_t pin);
static xRETURN_t ch32_set_interrupt_callback(void *driver_ctx, uint32_t pin, xGPIO_Driver_Interrupt_Callback_t callback, void *callback_ctx);

const xGPIO_Driver_Ops_t xGPIO_CH32H417_Driver_Ops = {
    .init = ch32_init,
    .deinit = ch32_deinit,
    .configure_pin = ch32_configure_pin,
    .pin_write = ch32_pin_write,
    .pin_read = ch32_pin_read,
    .pin_toggle = ch32_pin_toggle,
    .set_interrupt_callback = ch32_set_interrupt_callback,
};

// STATIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t ch32_init(void *driver_ctx, const xGPIO_Config_t *config)
{
    (void)config;
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    xRCC_Periph_t rcc_periph;
    if (ch32_ctx->gpiox == GPIOA)
    {
        rcc_periph = xRCC_PERIPH_GPIOA;
    }
    else if (ch32_ctx->gpiox == GPIOB)
    {
        rcc_periph = xRCC_PERIPH_GPIOB;
    }
    else if (ch32_ctx->gpiox == GPIOC)
    {
        rcc_periph = xRCC_PERIPH_GPIOC;
    }
    else if (ch32_ctx->gpiox == GPIOD)
    {
        rcc_periph = xRCC_PERIPH_GPIOD;
    }
    else if (ch32_ctx->gpiox == GPIOE)
    {
        rcc_periph = xRCC_PERIPH_GPIOE;
    }
    else if (ch32_ctx->gpiox == GPIOF)
    {
        rcc_periph = xRCC_PERIPH_GPIOF;
    }
    else
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    xRCC_Enable_Periph_Clock(rcc_periph);
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_AFIO);

    return xRETURN_OK;
}

static xRETURN_t ch32_deinit(void *driver_ctx)
{
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    // Disable clock gate if deinitializing
    xRCC_Periph_t rcc_periph;
    if (ch32_ctx->gpiox == GPIOA)
    {
        rcc_periph = xRCC_PERIPH_GPIOA;
    }
    else if (ch32_ctx->gpiox == GPIOB)
    {
        rcc_periph = xRCC_PERIPH_GPIOB;
    }
    else if (ch32_ctx->gpiox == GPIOC)
    {
        rcc_periph = xRCC_PERIPH_GPIOC;
    }
    else if (ch32_ctx->gpiox == GPIOD)
    {
        rcc_periph = xRCC_PERIPH_GPIOD;
    }
    else if (ch32_ctx->gpiox == GPIOE)
    {
        rcc_periph = xRCC_PERIPH_GPIOE;
    }
    else if (ch32_ctx->gpiox == GPIOF)
    {
        rcc_periph = xRCC_PERIPH_GPIOF;
    }
    else
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    xRCC_Disable_Periph_Clock(rcc_periph);
    return xRETURN_OK;
}

static xRETURN_t ch32_configure_pin(void *driver_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config)
{
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL) || (pin_config == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }
    if (pin >= 16U)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    uint8_t cnf_mode = 0U;
    switch (pin_config->mode)
    {
        case xGPIO_PIN_MODE_INPUT:
            if (pin_config->pull == xGPIO_PIN_PULL_NONE)
            {
                cnf_mode = 0x4U; // Floating input: CNF=01, MODE=00
            }
            else
            {
                cnf_mode = 0x8U; // Pull-up/pull-down: CNF=10, MODE=00
            }
            break;
        case xGPIO_PIN_MODE_OUTPUT_PUSH_PULL:
            cnf_mode = 0x3U; // CNF=00, MODE=11
            break;
        case xGPIO_PIN_MODE_OUTPUT_OPEN_DRAIN:
            cnf_mode = 0x7U; // CNF=01, MODE=11
            break;
        case xGPIO_PIN_MODE_ALTERNATE_PUSH_PULL:
            cnf_mode = 0xBU; // CNF=10, MODE=11
            break;
        case xGPIO_PIN_MODE_ALTERNATE_OPEN_DRAIN:
            cnf_mode = 0xFU; // CNF=11, MODE=11
            break;
        case xGPIO_PIN_MODE_ANALOG:
            cnf_mode = 0x0U; // CNF=00, MODE=00
            break;
        default:
            return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    // Configure CFGLR / CFGHR
    uint32_t register_val;
    if (pin < 8U)
    {
        uint32_t shift = pin * 4U;
        register_val = ch32_ctx->gpiox->CFGLR;
        register_val &= ~(0xFU << shift);
        register_val |= ((uint32_t)cnf_mode) << shift;
        ch32_ctx->gpiox->CFGLR = register_val;
    }
    else
    {
        uint32_t shift = (pin - 8U) * 4U;
        register_val = ch32_ctx->gpiox->CFGHR;
        register_val &= ~(0xFU << shift);
        register_val |= ((uint32_t)cnf_mode) << shift;
        ch32_ctx->gpiox->CFGHR = register_val;
    }

    // Configure Speed register
    uint8_t speed_val = 0U;
    switch (pin_config->speed)
    {
        case xGPIO_PIN_SPEED_LOW:
            speed_val = 0x0U;
            break;
        case xGPIO_PIN_SPEED_MEDIUM:
            speed_val = 0x1U;
            break;
        case xGPIO_PIN_SPEED_HIGH:
            speed_val = 0x2U;
            break;
        case xGPIO_PIN_SPEED_VERY_HIGH:
            speed_val = 0x3U;
            break;
        default:
            break;
    }
    uint32_t speed_shift = pin * 2U;
    register_val = ch32_ctx->gpiox->SPEED;
    register_val &= ~(0x3U << speed_shift);
    register_val |= ((uint32_t)speed_val) << speed_shift;
    ch32_ctx->gpiox->SPEED = register_val;

    // Handle Pull-Up / Pull-Down OUTDR register manipulation
    if (pin_config->mode == xGPIO_PIN_MODE_INPUT)
    {
        if (pin_config->pull == xGPIO_PIN_PULL_UP)
        {
            ch32_ctx->gpiox->BSHR = 1UL << pin;
        }
        else if (pin_config->pull == xGPIO_PIN_PULL_DOWN)
        {
            ch32_ctx->gpiox->BCR = 1UL << pin;
        }
    }

    // Configure AF mux if alternate function
    if ((pin_config->mode == xGPIO_PIN_MODE_ALTERNATE_PUSH_PULL) ||
        (pin_config->mode == xGPIO_PIN_MODE_ALTERNATE_OPEN_DRAIN))
    {
        uint32_t af_num = pin_config->alternate_function;
        if (af_num > 15U)
        {
            return xRETURN_xERR_xGPIO_INVALID_ARG;
        }
        GPIO_TypeDef *gpiox = ch32_ctx->gpiox;
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
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_pin_write(void *driver_ctx, uint32_t pin, bool level)
{
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }
    if (pin >= 16U)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    if (level)
    {
        ch32_ctx->gpiox->BSHR = 1UL << pin;
    }
    else
    {
        ch32_ctx->gpiox->BCR = 1UL << pin;
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_pin_read(void *driver_ctx, uint32_t pin, bool *level)
{
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL) || (level == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }
    if (pin >= 16U)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    *level = (ch32_ctx->gpiox->INDR & (1UL << pin)) != 0U;
    return xRETURN_OK;
}

static xRETURN_t ch32_pin_toggle(void *driver_ctx, uint32_t pin)
{
    xGPIO_CH32H417_Context_t *ch32_ctx = (xGPIO_CH32H417_Context_t *)driver_ctx;
    if ((ch32_ctx == NULL) || (ch32_ctx->gpiox == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }
    if (pin >= 16U)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    uint32_t odr = ch32_ctx->gpiox->OUTDR;
    if ((odr & (1UL << pin)) != 0U)
    {
        ch32_ctx->gpiox->BCR = 1UL << pin;
    }
    else
    {
        ch32_ctx->gpiox->BSHR = 1UL << pin;
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_set_interrupt_callback(void *driver_ctx, uint32_t pin, xGPIO_Driver_Interrupt_Callback_t callback, void *callback_ctx)
{
    (void)driver_ctx;
    (void)pin;
    (void)callback;
    (void)callback_ctx;
    // Interrupt callback configuration stub
    return xRETURN_xERR_xGPIO_UNSUPPORTED;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// EOF /////////////////////////////////////////////////////////////////////////////
