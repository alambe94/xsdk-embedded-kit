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

// @file xgpio.c
// @brief xGPIO core abstraction layer implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio.h"
#include "xgpio_trace.h"

// DEBUG

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void core_interrupt_dispatcher(void *callback_ctx, uint32_t pin);

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xGPIO_Init(xGPIO_Context_t *gpio_ctx, const xGPIO_Instance_t *instance, const xGPIO_Config_t *config)
{
    if ((gpio_ctx == NULL) || (instance == NULL) || (instance->ops == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    (void)memset(gpio_ctx, 0, sizeof(*gpio_ctx));

    gpio_ctx->ops = instance->ops;
    gpio_ctx->driver_ctx = instance->driver_ctx;
    if (config != NULL)
    {
        gpio_ctx->config = *config;
    }

    xRETURN_t ret = xRETURN_OK;
    if (gpio_ctx->ops->init != NULL)
    {
        ret = gpio_ctx->ops->init(gpio_ctx->driver_ctx, config);
    }

    if (ret == xRETURN_OK)
    {
        gpio_ctx->is_initialized = true;
        xGPIO_TRACE_E0(gpio_ctx, xGPIO_TRACE_CODE_INIT);
    }
    else
    {
        gpio_ctx->is_initialized = false;
    }

    return ret;
}

xRETURN_t xGPIO_Deinit(xGPIO_Context_t *gpio_ctx)
{
    if (gpio_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    xRETURN_t ret = xRETURN_OK;
    if ((gpio_ctx->ops != NULL) && (gpio_ctx->ops->deinit != NULL))
    {
        ret = gpio_ctx->ops->deinit(gpio_ctx->driver_ctx);
    }

    if (ret == xRETURN_OK)
    {
        xGPIO_TRACE_E0(gpio_ctx, xGPIO_TRACE_CODE_DEINIT);
        (void)memset(gpio_ctx, 0, sizeof(*gpio_ctx));
    }
    else
    {
        gpio_ctx->is_initialized = false;
    }

    return ret;
}

xRETURN_t xGPIO_Configure_Pin(xGPIO_Context_t *gpio_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config)
{
    if ((gpio_ctx == NULL) || (pin_config == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    if ((gpio_ctx->ops == NULL) || (gpio_ctx->ops->configure_pin == NULL))
    {
        return xRETURN_xERR_xGPIO_UNSUPPORTED;
    }

    xRETURN_t ret = gpio_ctx->ops->configure_pin(gpio_ctx->driver_ctx, pin, pin_config);
    if (ret == xRETURN_OK)
    {
        xGPIO_TRACE_E2(gpio_ctx, xGPIO_TRACE_CODE_CONFIGURE_PIN, pin, pin_config->mode);
    }
    return ret;
}

xRETURN_t xGPIO_Pin_Write(xGPIO_Context_t *gpio_ctx, uint32_t pin, bool level)
{
    if (gpio_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    if ((gpio_ctx->ops == NULL) || (gpio_ctx->ops->pin_write == NULL))
    {
        return xRETURN_xERR_xGPIO_UNSUPPORTED;
    }

    xRETURN_t ret = gpio_ctx->ops->pin_write(gpio_ctx->driver_ctx, pin, level);
    if (ret == xRETURN_OK)
    {
        xGPIO_TRACE_E2(gpio_ctx, xGPIO_TRACE_CODE_PIN_WRITE, pin, level);
    }
    return ret;
}

xRETURN_t xGPIO_Pin_Read(xGPIO_Context_t *gpio_ctx, uint32_t pin, bool *level)
{
    if ((gpio_ctx == NULL) || (level == NULL))
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    if ((gpio_ctx->ops == NULL) || (gpio_ctx->ops->pin_read == NULL))
    {
        return xRETURN_xERR_xGPIO_UNSUPPORTED;
    }

    xRETURN_t ret = gpio_ctx->ops->pin_read(gpio_ctx->driver_ctx, pin, level);
    if (ret == xRETURN_OK)
    {
        xGPIO_TRACE_E2(gpio_ctx, xGPIO_TRACE_CODE_PIN_READ, pin, *level);
    }
    return ret;
}

xRETURN_t xGPIO_Pin_Toggle(xGPIO_Context_t *gpio_ctx, uint32_t pin)
{
    if (gpio_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    if ((gpio_ctx->ops == NULL) || (gpio_ctx->ops->pin_toggle == NULL))
    {
        return xRETURN_xERR_xGPIO_UNSUPPORTED;
    }

    xRETURN_t ret = gpio_ctx->ops->pin_toggle(gpio_ctx->driver_ctx, pin);
    if (ret == xRETURN_OK)
    {
        xGPIO_TRACE_E1(gpio_ctx, xGPIO_TRACE_CODE_PIN_TOGGLE, pin);
    }
    return ret;
}

xRETURN_t xGPIO_Set_Interrupt_Callback(xGPIO_Context_t *gpio_ctx, uint32_t pin, xGPIO_Interrupt_Callback_t callback, void *user_ctx)
{
    if (gpio_ctx == NULL)
    {
        return xRETURN_xERR_xGPIO_NULL_POINTER;
    }

    if (!gpio_ctx->is_initialized)
    {
        return xRETURN_xERR_xGPIO_NOT_INITIALIZED;
    }

    if (pin >= 32U)
    {
        return xRETURN_xERR_xGPIO_INVALID_ARG;
    }

    if ((gpio_ctx->ops == NULL) || (gpio_ctx->ops->set_interrupt_callback == NULL))
    {
        return xRETURN_xERR_xGPIO_UNSUPPORTED;
    }

    gpio_ctx->callbacks[pin] = callback;
    gpio_ctx->user_ctxs[pin] = user_ctx;

    return gpio_ctx->ops->set_interrupt_callback(gpio_ctx->driver_ctx, pin, callback ? core_interrupt_dispatcher : NULL,
                                                 callback ? gpio_ctx : NULL);
}

static void core_interrupt_dispatcher(void *callback_ctx, uint32_t pin)
{
    xGPIO_Context_t *gpio_ctx = (xGPIO_Context_t *)callback_ctx;
    if ((gpio_ctx != NULL) && (pin < 32U))
    {
        if (gpio_ctx->callbacks[pin] != NULL)
        {
            gpio_ctx->callbacks[pin](gpio_ctx, pin, gpio_ctx->user_ctxs[pin]);
        }
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
