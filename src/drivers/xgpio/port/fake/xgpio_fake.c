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

// @file xgpio_fake.c
// @brief Fake xGPIO port implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio_fake.h"

// DEBUG

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t fake_init(void *driver_ctx, const xGPIO_Config_t *config);
static xRETURN_t fake_deinit(void *driver_ctx);
static xRETURN_t fake_configure_pin(void *driver_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config);
static xRETURN_t fake_pin_write(void *driver_ctx, uint32_t pin, bool level);
static xRETURN_t fake_pin_read(void *driver_ctx, uint32_t pin, bool *level);
static xRETURN_t fake_pin_toggle(void *driver_ctx, uint32_t pin);
static xRETURN_t fake_set_interrupt_callback(void *driver_ctx, uint32_t pin, xGPIO_Driver_Interrupt_Callback_t callback, void *callback_ctx);

const xGPIO_Driver_Ops_t xGPIO_Fake_Driver_Ops = {
    .init = fake_init,
    .deinit = fake_deinit,
    .configure_pin = fake_configure_pin,
    .pin_write = fake_pin_write,
    .pin_read = fake_pin_read,
    .pin_toggle = fake_pin_toggle,
    .set_interrupt_callback = fake_set_interrupt_callback,
};

// STATIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t fake_init(void *driver_ctx, const xGPIO_Config_t *config)
{
    (void)config;
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < XGPIO_FAKE_MAX_PINS; i++)
    {
        fake_ctx->pins_level[i] = false;
        fake_ctx->pins_config[i].mode = xGPIO_PIN_MODE_INPUT;
        fake_ctx->pins_config[i].pull = xGPIO_PIN_PULL_NONE;
        fake_ctx->pins_config[i].speed = xGPIO_PIN_SPEED_LOW;
        fake_ctx->pins_config[i].alternate_function = 0U;
        fake_ctx->callbacks[i] = NULL;
        fake_ctx->callback_contexts[i] = NULL;
    }

    fake_ctx->is_initialized = true;
    return xRETURN_OK;
}

static xRETURN_t fake_deinit(void *driver_ctx)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    fake_ctx->is_initialized = false;
    return xRETURN_OK;
}

static xRETURN_t fake_configure_pin(void *driver_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if ((fake_ctx == NULL) || (pin_config == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (pin >= XGPIO_FAKE_MAX_PINS)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    fake_ctx->pins_config[pin] = *pin_config;
    return xRETURN_OK;
}

static xRETURN_t fake_pin_write(void *driver_ctx, uint32_t pin, bool level)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (pin >= XGPIO_FAKE_MAX_PINS)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    if ((fake_ctx->pins_config[pin].mode != xGPIO_PIN_MODE_OUTPUT_PUSH_PULL) &&
        (fake_ctx->pins_config[pin].mode != xGPIO_PIN_MODE_OUTPUT_OPEN_DRAIN))
    {
        return xRETURN_xERR_xGPIO_INVALID_STATE;
    }

    fake_ctx->pins_level[pin] = level;
    return xRETURN_OK;
}

static xRETURN_t fake_pin_read(void *driver_ctx, uint32_t pin, bool *level)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if ((fake_ctx == NULL) || (level == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (pin >= XGPIO_FAKE_MAX_PINS)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    *level = fake_ctx->pins_level[pin];
    return xRETURN_OK;
}

static xRETURN_t fake_pin_toggle(void *driver_ctx, uint32_t pin)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (pin >= XGPIO_FAKE_MAX_PINS)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    if ((fake_ctx->pins_config[pin].mode != xGPIO_PIN_MODE_OUTPUT_PUSH_PULL) &&
        (fake_ctx->pins_config[pin].mode != xGPIO_PIN_MODE_OUTPUT_OPEN_DRAIN))
    {
        return xRETURN_xERR_xGPIO_INVALID_STATE;
    }

    fake_ctx->pins_level[pin] = !fake_ctx->pins_level[pin];
    return xRETURN_OK;
}

static xRETURN_t fake_set_interrupt_callback(void *driver_ctx, uint32_t pin, xGPIO_Driver_Interrupt_Callback_t callback, void *callback_ctx)
{
    xGPIO_Fake_Context_t *fake_ctx = (xGPIO_Fake_Context_t *)driver_ctx;
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (pin >= XGPIO_FAKE_MAX_PINS)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    fake_ctx->callbacks[pin] = callback;
    fake_ctx->callback_contexts[pin] = callback_ctx;
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xGPIO_Fake_Trigger_Interrupt(xGPIO_Fake_Context_t *fake_ctx, uint32_t pin)
{
    if ((fake_ctx != NULL) && (pin < XGPIO_FAKE_MAX_PINS))
    {
        if (fake_ctx->callbacks[pin] != NULL)
        {
            fake_ctx->callbacks[pin](fake_ctx->callback_contexts[pin], pin);
        }
    }
}

void xGPIO_Fake_Set_Input_Level(xGPIO_Fake_Context_t *fake_ctx, uint32_t pin, bool level)
{
    if ((fake_ctx != NULL) && (pin < XGPIO_FAKE_MAX_PINS))
    {
        fake_ctx->pins_level[pin] = level;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
