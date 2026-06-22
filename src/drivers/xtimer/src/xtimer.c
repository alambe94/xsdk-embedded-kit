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

// @file xtimer.c
// @brief xTIMER core abstraction layer implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer.h"
#include "xtimer_trace.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void core_event_sink(void *callback_ctx);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static void core_event_sink(void *callback_ctx)
{
    xTIMER_Context_t *timer_ctx = (xTIMER_Context_t *)callback_ctx;
    if (timer_ctx == NULL)
    {
        return;
    }

    xTIMER_TRACE_E0(timer_ctx, xTIMER_TRACE_CODE_CALLBACK);

    if (timer_ctx->callback != NULL)
    {
        timer_ctx->callback(timer_ctx, timer_ctx->user_ctx);
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xTIMER_Init(xTIMER_Context_t *timer_ctx, const xTIMER_Instance_t *instance, const xTIMER_Config_t *config)
{
    if ((timer_ctx == NULL) || (instance == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if ((instance->ops == NULL) || (instance->driver_ctx == NULL) || (instance->ops->init == NULL) || (instance->ops->deinit == NULL) ||
        (instance->ops->start == NULL) || (instance->ops->stop == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    (void)memset(timer_ctx, 0, sizeof(*timer_ctx));

    xRETURN_t status = instance->ops->init(instance->driver_ctx, config);
    if (status != xRETURN_OK)
    {
        return status;
    }

    timer_ctx->ops = instance->ops;
    timer_ctx->driver_ctx = instance->driver_ctx;
    timer_ctx->config = *config;
    timer_ctx->is_initialized = true;

    xTIMER_TRACE_E2(timer_ctx, xTIMER_TRACE_CODE_INIT, config->period_us, config->module_clk_hz);

    return xRETURN_OK;
}

xRETURN_t xTIMER_Deinit(xTIMER_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    xRETURN_t status = xRETURN_OK;
    if (timer_ctx->ops->deinit != NULL)
    {
        status = timer_ctx->ops->deinit(timer_ctx->driver_ctx);
    }

    if (status != xRETURN_OK)
    {
        return status;
    }

    xTIMER_TRACE_E0(timer_ctx, xTIMER_TRACE_CODE_DEINIT);

    (void)memset(timer_ctx, 0, sizeof(*timer_ctx));

    return xRETURN_OK;
}

xRETURN_t xTIMER_Start(xTIMER_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    xRETURN_t status = timer_ctx->ops->start(timer_ctx->driver_ctx);
    if (status == xRETURN_OK)
    {
        xTIMER_TRACE_E0(timer_ctx, xTIMER_TRACE_CODE_START);
    }

    return status;
}

xRETURN_t xTIMER_Stop(xTIMER_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    xRETURN_t status = timer_ctx->ops->stop(timer_ctx->driver_ctx);
    if (status == xRETURN_OK)
    {
        xTIMER_TRACE_E0(timer_ctx, xTIMER_TRACE_CODE_STOP);
    }

    return status;
}

xRETURN_t xTIMER_Get_Count(xTIMER_Context_t *timer_ctx, uint32_t *count)
{
    if ((timer_ctx == NULL) || (count == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    if (timer_ctx->ops->get_count == NULL)
    {
        return xRETURN_xERR_xTIMER_UNSUPPORTED;
    }

    xRETURN_t status = timer_ctx->ops->get_count(timer_ctx->driver_ctx, count);
    if (status == xRETURN_OK)
    {
        xTIMER_TRACE_E1(timer_ctx, xTIMER_TRACE_CODE_GET_COUNT, *count);
    }

    return status;
}

xRETURN_t xTIMER_Clear_IRQ(xTIMER_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    if (timer_ctx->ops->clear_irq == NULL)
    {
        return xRETURN_xERR_xTIMER_UNSUPPORTED;
    }

    xRETURN_t status = timer_ctx->ops->clear_irq(timer_ctx->driver_ctx);
    if (status == xRETURN_OK)
    {
        xTIMER_TRACE_E0(timer_ctx, xTIMER_TRACE_CODE_CLEAR_IRQ);
    }

    return status;
}

xRETURN_t xTIMER_Set_Periodic_Callback(xTIMER_Context_t *timer_ctx, xTIMER_Callback_t callback, void *user_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    if (!timer_ctx->is_initialized)
    {
        return xRETURN_xERR_xTIMER_NOT_INITIALIZED;
    }

    timer_ctx->callback = callback;
    timer_ctx->user_ctx = user_ctx;

    if (timer_ctx->ops->set_event_callback != NULL)
    {
        if (callback != NULL)
        {
            return timer_ctx->ops->set_event_callback(timer_ctx->driver_ctx, core_event_sink, timer_ctx);
        }
        else
        {
            return timer_ctx->ops->set_event_callback(timer_ctx->driver_ctx, NULL, NULL);
        }
    }

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
