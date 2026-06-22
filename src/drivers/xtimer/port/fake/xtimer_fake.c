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

// @file xtimer_fake.c
// @brief Host-based fake timer hardware port implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_fake.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t fake_timer_init(void *driver_ctx, const xTIMER_Config_t *config);
static xRETURN_t fake_timer_deinit(void *driver_ctx);
static xRETURN_t fake_timer_start(void *driver_ctx);
static xRETURN_t fake_timer_stop(void *driver_ctx);
static xRETURN_t fake_timer_get_count(void *driver_ctx, uint32_t *count);
static xRETURN_t fake_timer_clear_irq(void *driver_ctx);
static xRETURN_t fake_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx);

// DRIVER OPS //////////////////////////////////////////////////////////////////////
const xTIMER_Driver_Ops_t xTIMER_Fake_Driver_Ops = {
    .init = fake_timer_init,
    .deinit = fake_timer_deinit,
    .start = fake_timer_start,
    .stop = fake_timer_stop,
    .get_count = fake_timer_get_count,
    .clear_irq = fake_timer_clear_irq,
    .set_event_callback = fake_timer_set_event_callback,
};

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xTIMER_Fake_Trigger_Interrupt(xTIMER_Fake_Context_t *ctx)
{
    if ((ctx != NULL) && ctx->is_initialized && ctx->is_started && (ctx->callback != NULL))
    {
        ctx->callback(ctx->callback_ctx);
    }
}

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static xRETURN_t fake_timer_init(void *driver_ctx, const xTIMER_Config_t *config)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->count = 0;
    ctx->is_initialized = true;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_deinit(void *driver_ctx)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->is_initialized = false;
    ctx->is_started = false;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_start(void *driver_ctx)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->is_started = true;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_stop(void *driver_ctx)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->is_started = false;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_get_count(void *driver_ctx, uint32_t *count)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if ((ctx == NULL) || (count == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->count++;
    *count = ctx->count;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_clear_irq(void *driver_ctx)
{
    (void)driver_ctx;
    return xRETURN_OK;
}

static xRETURN_t fake_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xTIMER_Fake_Context_t *ctx = (xTIMER_Fake_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->callback = callback;
    ctx->callback_ctx = callback_ctx;
    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
