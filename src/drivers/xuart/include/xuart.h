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

// @file xuart.h
// @brief Public xUART controller API.
//

#ifndef XUART_H
#define XUART_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xuart_config.h"
#include "xuart_driver.h"
#include "xuart_trace.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    struct xUART_Context_t
    {
        const xUART_Driver_Ops_t *ops;
        void *driver_ctx;
        uint8_t port;

        xUART_Config_t config;
        xUART_Callbacks_t callbacks;
        void *user_ctx;

        bool is_initialized;
        bool is_started;
        bool is_tx_busy;
        bool is_rx_busy;
        xRETURN_t last_tx_error;
        xRETURN_t last_rx_error;

#if xTRACE_ENABLE && xUART_TRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx;
#endif
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // Attach a trace context. Call after xUART_Init, before xUART_Start.
    // Passing NULL detaches tracing. No-op when xUART_TRACE_ENABLE is 0.
    static inline xRETURN_t xUART_Trace_Init(xUART_Context_t *uart_ctx, struct xTRACE_Context_t *trace_ctx)
    {
#if xTRACE_ENABLE && xUART_TRACE_ENABLE
        uart_ctx->trace_ctx = trace_ctx;
#else
    (void)uart_ctx;
    (void)trace_ctx;
#endif
        return xRETURN_OK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    xRETURN_t xUART_Init(xUART_Context_t *uart_ctx, const xUART_Config_t *config);

    xRETURN_t xUART_Deinit(xUART_Context_t *uart_ctx);

    xRETURN_t xUART_Start(xUART_Context_t *uart_ctx, const xUART_Start_Config_t *start_config);

    xRETURN_t xUART_Stop(xUART_Context_t *uart_ctx);

    xRETURN_t xUART_Set_Callback(xUART_Context_t *uart_ctx, const xUART_Callbacks_t *callbacks, void *user_ctx);

    xRETURN_t xUART_Transmit(xUART_Context_t *uart_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms);

    xRETURN_t xUART_Receive(xUART_Context_t *uart_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms);

    xRETURN_t xUART_Transmit_Async(xUART_Context_t *uart_ctx, const uint8_t *buffer, uint32_t length);

    xRETURN_t xUART_Receive_Async(xUART_Context_t *uart_ctx, uint8_t *buffer, uint32_t length);

    xRETURN_t xUART_Abort_Tx(xUART_Context_t *uart_ctx);

    xRETURN_t xUART_Abort_Rx(xUART_Context_t *uart_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_H
// EOF /////////////////////////////////////////////////////////////////////////////
