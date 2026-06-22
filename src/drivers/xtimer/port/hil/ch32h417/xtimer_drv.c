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
// @brief CH32H417 Timer hardware port implementation for the xTIMER driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xtimer_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t ch32_timer_init(void *driver_ctx, const xTIMER_Config_t *config);
static xRETURN_t ch32_timer_deinit(void *driver_ctx);
static xRETURN_t ch32_timer_start(void *driver_ctx);
static xRETURN_t ch32_timer_stop(void *driver_ctx);
static xRETURN_t ch32_timer_get_count(void *driver_ctx, uint32_t *count);
static xRETURN_t ch32_timer_clear_irq(void *driver_ctx);
static xRETURN_t ch32_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx);

// DRIVER OPS //////////////////////////////////////////////////////////////////////
const xTIMER_Driver_Ops_t xTIMER_CH32H417_Driver_Ops = {
    .init = ch32_timer_init,
    .deinit = ch32_timer_deinit,
    .start = ch32_timer_start,
    .stop = ch32_timer_stop,
    .get_count = ch32_timer_get_count,
    .clear_irq = ch32_timer_clear_irq,
    .set_event_callback = ch32_timer_set_event_callback,
};

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static xRETURN_t ch32_timer_init(void *driver_ctx, const xTIMER_Config_t *config)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    uint32_t base_addr = ctx->base_addr;
    TIM_TypeDef *tim = (TIM_TypeDef *)base_addr;

    // 1. Enable peripheral clock and reset timer block dynamically
    if (base_addr == TIM1_BASE)
    {
        RCC->HB2PCENR |= RCC_TIM1EN;
        RCC->HB2PRSTR |= RCC_TIM1RST;
        RCC->HB2PRSTR &= ~RCC_TIM1RST;
    }
    else if (base_addr == TIM2_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM2EN;
        RCC->HB1PRSTR |= RCC_TIM2RST;
        RCC->HB1PRSTR &= ~RCC_TIM2RST;
    }
    else if (base_addr == TIM3_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM3EN;
        RCC->HB1PRSTR |= RCC_TIM3RST;
        RCC->HB1PRSTR &= ~RCC_TIM3RST;
    }
    else if (base_addr == TIM4_BASE)
    {
        RCC->HB1PCENR |= RCC_TIM4EN;
        RCC->HB1PRSTR |= RCC_TIM4RST;
        RCC->HB1PRSTR &= ~RCC_TIM4RST;
    }

    // Disable timer count
    tim->CTLR1 = 0U;
    tim->CTLR2 = 0U;

    // 2. Compute prescaler and auto-reload registers
    uint32_t psc = config->module_clk_hz / 1000000U;
    uint32_t arr = config->period_us;
    while (arr > 65535U)
    {
        psc *= 2U;
        arr /= 2U;
    }
    if (psc > 0U)
    {
        psc--;
    }
    if (arr > 0U)
    {
        arr--;
    }

    tim->PSC   = (uint16_t)psc;
    tim->ATRLR = (uint16_t)arr;

    // Clear update event flag and configure UIE interrupt
    tim->INTFR      = ~(uint16_t)TIM_UIF;
    tim->DMAINTENR  = TIM_UIE;

    // Seed the counter registers immediately
    tim->SWEVGR = TIM_UG;

    ctx->is_initialized = true;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_deinit(void *driver_ctx)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    TIM_TypeDef *tim = (TIM_TypeDef *)ctx->base_addr;
    tim->CTLR1    &= ~(uint16_t)TIM_CEN;  // stop counter
    tim->DMAINTENR = 0U;                   // disable all timer interrupts (UIE etc.)

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->callback       = NULL;
    ctx->callback_ctx   = NULL;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_start(void *driver_ctx)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    TIM_TypeDef *tim = (TIM_TypeDef *)ctx->base_addr;
    tim->CTLR1 |= TIM_CEN;
    ctx->is_started = true;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_stop(void *driver_ctx)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    TIM_TypeDef *tim = (TIM_TypeDef *)ctx->base_addr;
    tim->CTLR1 &= ~(uint16_t)TIM_CEN;
    ctx->is_started = false;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_get_count(void *driver_ctx, uint32_t *count)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (count == NULL))
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    TIM_TypeDef *tim = (TIM_TypeDef *)ctx->base_addr;
    *count = tim->CNT;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_clear_irq(void *driver_ctx)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    TIM_TypeDef *tim = (TIM_TypeDef *)ctx->base_addr;
    tim->INTFR = ~(uint16_t)TIM_UIF;
    return xRETURN_OK;
}

static xRETURN_t ch32_timer_set_event_callback(void *driver_ctx, xTIMER_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xTIMER_CH32H417_Context_t *ctx = (xTIMER_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xTIMER_NULL_POINTER;
    }

    ctx->callback = callback;
    ctx->callback_ctx = callback_ctx;
    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
