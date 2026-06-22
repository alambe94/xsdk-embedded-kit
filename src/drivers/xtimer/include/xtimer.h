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

// @file xtimer.h
// @brief Public xTIMER driver API.
//

#ifndef XTIMER_H
#define XTIMER_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_config.h"
#include "xtimer_driver.h"
#include "xtimer_trace.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    struct xTIMER_Context_t
    {
        const xTIMER_Driver_Ops_t *ops;
        void *driver_ctx;
        xTIMER_Config_t config;
        xTIMER_Callback_t callback;
        void *user_ctx;
        bool is_initialized;
#if xTRACE_ENABLE && xTIMER_TRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx;
#endif
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Attach a trace context. Call after xTIMER_Init.
    // Passing NULL detaches tracing. No-op when xTIMER_TRACE_ENABLE is 0.
    static inline xRETURN_t xTIMER_Trace_Init(xTIMER_Context_t *timer_ctx, struct xTRACE_Context_t *trace_ctx)
    {
#if xTRACE_ENABLE && xTIMER_TRACE_ENABLE
        timer_ctx->trace_ctx = trace_ctx;
#else
    (void)timer_ctx;
    (void)trace_ctx;
#endif
        return xRETURN_OK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xTIMER_Init(xTIMER_Context_t *timer_ctx, const xTIMER_Instance_t *instance, const xTIMER_Config_t *config);
    xRETURN_t xTIMER_Deinit(xTIMER_Context_t *timer_ctx);

    xRETURN_t xTIMER_Start(xTIMER_Context_t *timer_ctx);
    xRETURN_t xTIMER_Stop(xTIMER_Context_t *timer_ctx);

    xRETURN_t xTIMER_Get_Count(xTIMER_Context_t *timer_ctx, uint32_t *count);
    xRETURN_t xTIMER_Clear_IRQ(xTIMER_Context_t *timer_ctx);

    xRETURN_t xTIMER_Set_Periodic_Callback(xTIMER_Context_t *timer_ctx, xTIMER_Callback_t callback, void *user_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTIMER_H
// EOF /////////////////////////////////////////////////////////////////////////////
