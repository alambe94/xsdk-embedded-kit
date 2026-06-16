// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xrtos_timer.c
// @brief xRTOS software timer: Init, Start, Stop.
//
// Timer callbacks fire in the system tick ISR via xRTOS_Tick_Increment_From_ISR.
// Callers must ensure callbacks are ISR-safe (short, non-blocking).
//
// MISRA C:2012 Rule 15.5 deviation: multiple return points are used throughout
// for early exit on invalid arguments. This is an explicit project-level
// deviation documented in xsdk_style_guide.md.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_config.h"
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_timer.h"
#include "xrtos_private.h"
#include "xrtos_scheduler.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"
#include "xrtos_sem.h"
#include "xrtos_tick.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t timer_validate_registered(const xRTOS_Kernel_Context_t *kernel, const xRTOS_Timer_Context_t *timer_ctx);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////

static xRETURN_t timer_validate_registered(const xRTOS_Kernel_Context_t *kernel, const xRTOS_Timer_Context_t *timer_ctx)
{
    xASSERT(kernel != NULL, "kernel is NULL");
    xASSERT(timer_ctx != NULL, "timer_ctx is NULL");

    if (timer_ctx->timer_id >= xRTOS_MAX_TIMERS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if (kernel->timer_table[timer_ctx->timer_id] != timer_ctx)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Timer_Init(xRTOS_Timer_Context_t *timer_ctx, const xRTOS_Timer_Config_t *config)
{
    if ((timer_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (config->callback == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (config->timer_id >= xRTOS_MAX_TIMERS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if (config->period_ticks == 0U)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    if (kernel->timer_table[config->timer_id] != NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    timer_ctx->timer_id = config->timer_id;
    timer_ctx->callback = config->callback;
    timer_ctx->callback_arg = config->callback_arg;
    timer_ctx->period_ticks = config->period_ticks;
    timer_ctx->is_periodic = config->is_periodic;
    timer_ctx->expiry_tick = 0U;
    timer_ctx->name = config->name;

    kernel->timer_table[config->timer_id] = timer_ctx;

    xRTOS_Scheduler_Unlock(saved_state);

    if (config->name != NULL)
    {
        xRTOS_TRACE_NAME(kernel, xRTOS_TRACE_OBJ_TIMER, config->timer_id, config->name);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "timer initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Timer_Start(xRTOS_Timer_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    xRETURN_t ret = timer_validate_registered(kernel, timer_ctx);
    if (ret != xRETURN_xRTOS_OK)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }

    timer_ctx->expiry_tick = kernel->tick_count + timer_ctx->period_ticks;

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
    const bool first_active = xRTOS_Bitmap_Is_Empty(&kernel->active_timers_map);
#endif

    xRTOS_Bitmap_Set(&kernel->active_timers_map, timer_ctx->timer_id);
    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TIMER_START, timer_ctx->timer_id);

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
    if (first_active || xRTOS_Tick_Has_Expired(kernel->next_timer_expiry_tick, timer_ctx->expiry_tick))
    {
        kernel->next_timer_expiry_tick = timer_ctx->expiry_tick;
    }
#endif

    xRTOS_Scheduler_Unlock(saved_state);

    xRTOS_LOG(xRETURN_xRTOS_OK, "timer started");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Timer_Stop(xRTOS_Timer_Context_t *timer_ctx)
{
    if (timer_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    xRETURN_t ret = timer_validate_registered(kernel, timer_ctx);
    if (ret != xRETURN_xRTOS_OK)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }

    xRTOS_Bitmap_Clear(&kernel->active_timers_map, timer_ctx->timer_id);
    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TIMER_STOP, timer_ctx->timer_id);

    xRTOS_Scheduler_Unlock(saved_state);

    xRTOS_LOG(xRETURN_xRTOS_OK, "timer stopped");

    return xRETURN_xRTOS_OK;
}

#if (xRTOS_MAX_TIMERS > 0U) && (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
void xrtos_timer_daemon_entry(void *arg)
{
    (void)arg;
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel is NULL");

    for (;;)
    {
        // Block until signaled by the tick ISR
        (void)xRTOS_Sem_Take(&kernel->timer_sem, xRTOS_WAIT_FOREVER);

        // Process expired timers
        uint32_t saved_state = xRTOS_Scheduler_Lock();

        xRTOS_Bitmap_t timer_scan = kernel->active_timers_map;
        uint32_t timer_id = 0U;
        uint32_t current_tick = kernel->tick_count;

        while (xRTOS_Bitmap_Find_First_Set(&timer_scan, &timer_id) == xRETURN_xRTOS_OK)
        {
            xRTOS_Bitmap_Clear(&timer_scan, timer_id);

            if (timer_id >= xRTOS_MAX_TIMERS)
            {
                xRTOS_Bitmap_Clear(&kernel->active_timers_map, timer_id);
                continue;
            }

            struct xRTOS_Timer_Context_t *timer = kernel->timer_table[timer_id];
            if (timer == NULL)
            {
                xRTOS_Bitmap_Clear(&kernel->active_timers_map, timer_id);
                continue;
            }

            if (xRTOS_Tick_Has_Expired(current_tick, timer->expiry_tick))
            {
                // Temporarily unlock scheduler to execute the callback in task context with interrupts enabled.
                xRTOS_Scheduler_Unlock(saved_state);

                timer->callback(timer->callback_arg);
                xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TIMER_FIRE, timer_id);

                saved_state = xRTOS_Scheduler_Lock();

                // Recheck if the timer was not stopped/re-started during callback
                if (xRTOS_Bitmap_Is_Set(&kernel->active_timers_map, timer_id))
                {
                    if (timer->is_periodic)
                    {
                        timer->expiry_tick = kernel->tick_count + timer->period_ticks;
                    }
                    else
                    {
                        xRTOS_Bitmap_Clear(&kernel->active_timers_map, timer_id);
                        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TIMER_STOP, timer_id);
                    }
                }
            }
        }

        // Recompute next_timer_expiry_tick from the remaining active timers.
        xRTOS_Bitmap_t remaining = kernel->active_timers_map;
        uint32_t scan_id = 0U;
        bool found = false;
        while (xRTOS_Bitmap_Find_First_Set(&remaining, &scan_id) == xRETURN_xRTOS_OK)
        {
            xRTOS_Bitmap_Clear(&remaining, scan_id);
            if ((scan_id < xRTOS_MAX_TIMERS) && (kernel->timer_table[scan_id] != NULL))
            {
                uint32_t expiry = kernel->timer_table[scan_id]->expiry_tick;
                if (!found || xRTOS_Tick_Has_Expired(kernel->next_timer_expiry_tick, expiry))
                {
                    kernel->next_timer_expiry_tick = expiry;
                    found = true;
                }
            }
        }

        xRTOS_Scheduler_Unlock(saved_state);
    }
}
#endif

// EOF /////////////////////////////////////////////////////////////////////////////
