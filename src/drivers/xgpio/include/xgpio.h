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

// @file xgpio.h
// @brief Public xGPIO controller API.
//

#ifndef XGPIO_H
#define XGPIO_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xgpio_config.h"
#include "xgpio_driver.h"
#include "xgpio_trace.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    struct xGPIO_Context_t
    {
        const xGPIO_Driver_Ops_t *ops;
        void *driver_ctx;
        xGPIO_Config_t config;
        xGPIO_Interrupt_Callback_t callbacks[32];
        void *user_ctxs[32];
        bool is_initialized;
#if xTRACE_ENABLE && xGPIO_TRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx;
#endif
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // Attach a trace context. Call after xGPIO_Init.
    // Passing NULL detaches tracing. No-op when xGPIO_TRACE_ENABLE is 0.
    static inline xRETURN_t xGPIO_Trace_Init(xGPIO_Context_t *gpio_ctx, struct xTRACE_Context_t *trace_ctx)
    {
#if xTRACE_ENABLE && xGPIO_TRACE_ENABLE
        gpio_ctx->trace_ctx = trace_ctx;
#else
    (void)gpio_ctx;
    (void)trace_ctx;
#endif
        return xRETURN_OK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    xRETURN_t xGPIO_Init(xGPIO_Context_t *gpio_ctx, const xGPIO_Instance_t *instance, const xGPIO_Config_t *config);
    xRETURN_t xGPIO_Deinit(xGPIO_Context_t *gpio_ctx);
    xRETURN_t xGPIO_Configure_Pin(xGPIO_Context_t *gpio_ctx, uint32_t pin, const xGPIO_Pin_Config_t *pin_config);
    xRETURN_t xGPIO_Pin_Write(xGPIO_Context_t *gpio_ctx, uint32_t pin, bool level);
    xRETURN_t xGPIO_Pin_Read(xGPIO_Context_t *gpio_ctx, uint32_t pin, bool *level);
    xRETURN_t xGPIO_Pin_Toggle(xGPIO_Context_t *gpio_ctx, uint32_t pin);
    xRETURN_t xGPIO_Set_Interrupt_Callback(xGPIO_Context_t *gpio_ctx, uint32_t pin, xGPIO_Interrupt_Callback_t callback, void *user_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XGPIO_H
// EOF /////////////////////////////////////////////////////////////////////////////
