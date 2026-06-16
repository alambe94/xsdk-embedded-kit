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

// @file xrtos_tick.c
// @brief xRTOS tick counter, task delay, and timeout processing.
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
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_timer.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"
#include "xrtos_blocking.h"
#include "xrtos_private.h"
#include "xrtos_sem.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

uint32_t xRTOS_Tick_Get(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialized");
    xASSERT(kernel->is_initialized, "kernel not initialized");

    return kernel->tick_count;
}

xRETURN_t xRTOS_Task_Delay(uint32_t ticks)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if (!kernel->scheduler.is_started)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    if (xrtos_blocking_call_is_from_isr(kernel))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    xASSERT(ticks != xRTOS_WAIT_FOREVER, "Task_Delay called with WAIT_FOREVER");

    xRTOS_Task_Context_t *task_ctx = xRTOS_Scheduler_Current_Task();
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    if (ticks == 0U)
    {
        // Yield without blocking: request a reschedule and return immediately.
        uint32_t saved_state = xRTOS_Scheduler_Lock();
        kernel->scheduler.is_schedule_pending = true;
        xRTOS_Scheduler_Unlock(saved_state);
        kernel->port_ops->yield();
        return xRETURN_xRTOS_OK;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Arm the timeout: wake when tick_count reaches wake_tick.
    xrtos_blocking_prepare(kernel, task_ctx, ticks, NULL, NULL);

    // Block the current task. wait_map_ptr is NULL for pure delay (no sync object).
    xRETURN_t ret = xRTOS_Scheduler_Block_Current(NULL);
    if (ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    // On real hardware execution resumes here after the context switch returns
    // the task from its BLOCKED state. The block_status contains
    // xRETURN_xERR_xRTOS_TIMEOUT written by xRTOS_Tick_Increment_From_ISR,
    // which is the expected success path for a pure delay.
    return xRETURN_xRTOS_OK;
}

void xRTOS_Tick_Increment_From_ISR(bool *should_yield)
{
    xASSERT(should_yield != NULL, "should_yield is NULL");
    if (should_yield == NULL)
    {
        return;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialized");
    xASSERT(kernel->is_initialized, "kernel not initialized");

    // Advance the free-running tick counter (wraps at UINT32_MAX).
    kernel->tick_count++;

#if xRTOS_CONFIG_CPU_STATS_ENABLE
    {
        kernel->cpu_stats_ticks_total++;
        uint32_t cur = kernel->scheduler.current_task_id;
        if ((cur < xRTOS_MAX_TASKS) && (kernel->task_table[cur] != NULL))
        {
            kernel->task_table[cur]->ticks_running++;
        }
    }
#endif

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TICK, kernel->tick_count);

    xRTOS_LOG(xRETURN_xRTOS_OK, "tick");

    // Scan set bits in timeout_map for tasks whose wake_tick has arrived.
    // Iterate a snapshot so expiry processing does not corrupt the scan.
    xRTOS_Bitmap_t timeout_scan = kernel->timeout_map;
    uint32_t task_id = 0U;
    while (xRTOS_Bitmap_Find_First_Set(&timeout_scan, &task_id) == xRETURN_xRTOS_OK)
    {
        xRTOS_Bitmap_Clear(&timeout_scan, task_id);

        xRTOS_Task_Context_t *task_ctx = (task_id < xRTOS_MAX_TASKS) ? kernel->task_table[task_id] : NULL;
        if (task_ctx == NULL)
        {
            xRTOS_Bitmap_Clear(&kernel->timeout_map, task_id);
            continue;
        }

        if (xRTOS_Tick_Has_Expired(kernel->tick_count, task_ctx->wake_tick))
        {
            if (task_ctx->block_cleanup != NULL)
            {
                task_ctx->block_cleanup(task_id, task_ctx->block_cleanup_arg);
            }
            xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TASK_TIMEOUT, task_id);
            (void)xRTOS_Scheduler_Unblock(task_id, xRETURN_xERR_xRTOS_TIMEOUT);
        }
    }

#if (xRTOS_MAX_TIMERS > 0U)
#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
    // Task-deferred mode: wake the daemon if the earliest active timer has expired.
    // next_timer_expiry_tick is maintained by Timer_Start and the daemon; no bitmap scan needed.
    if (!xRTOS_Bitmap_Is_Empty(&kernel->active_timers_map) && xRTOS_Tick_Has_Expired(kernel->tick_count, kernel->next_timer_expiry_tick))
    {
        (void)xRTOS_Sem_Give_From_ISR(&kernel->timer_sem, should_yield);
    }
#else
    // Scan active software timers and fire any that have expired.
    // Iterate a snapshot so that one-shot deactivation does not corrupt the scan.
    xRTOS_Bitmap_t timer_scan = kernel->active_timers_map;
    uint32_t timer_id = 0U;
    while (xRTOS_Bitmap_Find_First_Set(&timer_scan, &timer_id) == xRETURN_xRTOS_OK)
    {
        xRTOS_Bitmap_Clear(&timer_scan, timer_id);

        struct xRTOS_Timer_Context_t *timer = (timer_id < xRTOS_MAX_TIMERS) ? kernel->timer_table[timer_id] : NULL;
        if (timer == NULL)
        {
            xRTOS_Bitmap_Clear(&kernel->active_timers_map, timer_id);
            continue;
        }

        if (xRTOS_Tick_Has_Expired(kernel->tick_count, timer->expiry_tick))
        {
            timer->callback(timer->callback_arg);
            xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TIMER_FIRE, timer_id);

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
#endif
#endif

#if xRTOS_CONFIG_ROUND_ROBIN_ENABLE
    // If another task is waiting at the same effective priority as the running
    // task, request a switch so they share the CPU in FIFO order.
    // xRTOS_Scheduler_Switch already moves the outgoing task to the tail.
    if (kernel->scheduler.is_started && (kernel->scheduler.ready_head[kernel->scheduler.current_priority] != NULL))
    {
        kernel->scheduler.is_schedule_pending = true;
    }
#endif

    // Report whether a context switch is outstanding.
    *should_yield = kernel->scheduler.is_schedule_pending;
}

// EOF /////////////////////////////////////////////////////////////////////////////
