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

// @file xtimer_drv.c
// @brief TI AM243x DMTimer hardware port implementation for the xTIMER driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xtimer_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////////
/* DMTimer register offsets (Keystone3 / AM64x / AM243x) */
#define TIOCP_CFG 0x10U
#define TISR      0x28U
#define TIER      0x2CU
#define TCLR      0x38U
#define TCRR      0x3CU
#define TLDR      0x40U

#define TCLR_ST  (1U << 0)
#define TCLR_AR  (1U << 1)
#define TIER_OVF (1U << 1)
#define TISR_OVF (1U << 1)

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t am243x_timer_init(void *driver_ctx, const xTIMER_Config_t *config);
static xRETURN_t am243x_timer_deinit(void *driver_ctx);
static xRETURN_t am243x_timer_start(void *driver_ctx);
static xRETURN_t am243x_timer_stop(void *driver_ctx);
static xRETURN_t am243x_timer_get_count(void *driver_ctx, uint32_t *count);
static xRETURN_t am243x_timer_clear_irq(void *driver_ctx);
static xRETURN_t am243x_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx);

// DRIVER OPS //////////////////////////////////////////////////////////////////////
const xTIMER_Driver_Ops_t xTIMER_AM243x_Driver_Ops = {
    .init = am243x_timer_init,
    .deinit = am243x_timer_deinit,
    .start = am243x_timer_start,
    .stop = am243x_timer_stop,
    .get_count = am243x_timer_get_count,
    .clear_irq = am243x_timer_clear_irq,
    .set_event_callback = am243x_timer_set_event_callback,
};

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static xRETURN_t am243x_timer_init(void *driver_ctx, const xTIMER_Config_t *config)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    uint32_t base_addr = ctx->base_addr;
    uint32_t ticks = (uint32_t)((uint64_t)config->module_clk_hz * config->period_us / 1000000U);
    uint32_t load  = 0xFFFFFFFFU - ticks + 1U;

    REG32(base_addr, TIOCP_CFG) = 0x1U;
    while ((REG32(base_addr, TIOCP_CFG) & 0x1U) != 0U)
    {
    }

    REG32(base_addr, TCLR) = 0U;
    REG32(base_addr, TLDR) = load;
    REG32(base_addr, TCRR) = load;
    REG32(base_addr, TISR) = TISR_OVF;
    REG32(base_addr, TIER) = TIER_OVF;

    ctx->is_initialized = true;
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_deinit(void *driver_ctx)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->is_initialized = false;
    ctx->is_started = false;
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_start(void *driver_ctx)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    REG32(ctx->base_addr, TCLR) = TCLR_ST | TCLR_AR;
    ctx->is_started = true;
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_stop(void *driver_ctx)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    REG32(ctx->base_addr, TCLR) &= ~(uint32_t)TCLR_ST;
    ctx->is_started = false;
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_get_count(void *driver_ctx, uint32_t *count)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if ((ctx == NULL) || (count == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    *count = REG32(ctx->base_addr, TCRR);
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_clear_irq(void *driver_ctx)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    REG32(ctx->base_addr, TISR) = TISR_OVF;
    return xRETURN_OK;
}

static xRETURN_t am243x_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xTIMER_AM243x_Context_t *ctx = (xTIMER_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->callback = callback;
    ctx->callback_ctx = callback_ctx;
    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
