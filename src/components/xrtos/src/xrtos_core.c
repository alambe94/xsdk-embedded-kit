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

// @file xrtos_core.c
// @brief xRTOS kernel lifecycle - init, start, static task registration,
//        task exit, and priority search.
//
// MISRA C:2012 Rule 15.5 deviation: multiple return points are used throughout
// for early exit on invalid arguments. This is an explicit project-level
// deviation documented in xsdk_style_guide.md.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_config.h"
#include "xrtos_core.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"
#include "xrtos_private.h"
#include "xrtos_sem.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// Single global kernel instance registered by xRTOS_Kernel_Init.
// NULL until xRTOS_Kernel_Init succeeds.
static xRTOS_Kernel_Context_t *g_kernel = NULL;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool is_kernel_ready(void);
static xRTOS_Task_Context_t *task_find_first_at_base_priority(uint32_t priority, const xRTOS_Task_Context_t *exclude_task);
static xRETURN_t validate_port_ops(const xRTOS_Port_Ops_t *port_ops);
static xRETURN_t validate_task_config(const xRTOS_Task_Config_t *task_config);

// Non-static accessor used by internal translation units via xrtos_private.h.
xRTOS_Kernel_Context_t *xrtos_kernel_get(void)
{
    return g_kernel;
}

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool is_kernel_ready(void)
{
    return (g_kernel != NULL) && (g_kernel->is_initialized);
}

static xRTOS_Task_Context_t *task_find_first_at_base_priority(uint32_t priority, const xRTOS_Task_Context_t *exclude_task)
{
    for (uint32_t i = 0U; i < xRTOS_MAX_TASKS; i++)
    {
        xRTOS_Task_Context_t *candidate = g_kernel->task_table[i];
        if ((candidate != NULL) && (candidate != exclude_task) && (candidate->base_priority == priority))
        {
            return candidate;
        }
    }

    return NULL;
}

static xRETURN_t validate_port_ops(const xRTOS_Port_Ops_t *port_ops)
{
    if (port_ops == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->init_task_stack == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->start_first_task == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->yield == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->disable_interrupts == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->enable_interrupts == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (port_ops->is_in_isr == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    return xRETURN_xRTOS_OK;
}

static xRETURN_t validate_task_config(const xRTOS_Task_Config_t *task_config)
{
    if (task_config->task_id >= xRTOS_MAX_TASKS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if (task_config->priority >= xRTOS_MAX_PRIORITIES)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if ((task_config->task_id == xRTOS_IDLE_TASK_ID) != (task_config->priority == xRTOS_IDLE_PRIORITY))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if (task_config->entry == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (task_config->stack_mem == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (task_config->stack_words < xRTOS_CONFIG_MIN_STACK_WORDS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Kernel_Init(xRTOS_Kernel_Context_t *kernel_ctx, const xRTOS_Port_Ops_t *port_ops)
{
    if (kernel_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    xRETURN_t ret = validate_port_ops(port_ops);
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }
    if ((g_kernel != NULL) && (g_kernel->is_initialized) && (g_kernel->scheduler.is_started) && (kernel_ctx != g_kernel))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    (void)memset(kernel_ctx, 0, sizeof(xRTOS_Kernel_Context_t));

    kernel_ctx->port_ops = port_ops;

    // Sentinel: no task is running before the scheduler starts.
    kernel_ctx->scheduler.current_task_id = xRTOS_INVALID_TASK_ID;
    kernel_ctx->scheduler.next_task_id = xRTOS_INVALID_TASK_ID;
    kernel_ctx->scheduler.current_priority = xRTOS_INVALID_TASK_ID;
    kernel_ctx->scheduler.next_priority = xRTOS_INVALID_TASK_ID;

    kernel_ctx->is_initialized = true;

    g_kernel = kernel_ctx;

#if (xRTOS_MAX_TIMERS > 0U) && (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
    // Initialize binary semaphore for the Timer Daemon Task, initially locked
    ret = xRTOS_Sem_Init(&kernel_ctx->timer_sem, 0U, 1U, "TimerSem");
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }

    // Create Timer Daemon Task
    xRTOS_Task_Config_t timer_task_config = {.task_id = xRTOS_TIMER_TASK_ID,
                                             .priority = xRTOS_CONFIG_TIMER_TASK_PRIORITY,
                                             .entry = xrtos_timer_daemon_entry,
                                             .entry_arg = NULL,
                                             .stack_mem = kernel_ctx->timer_task_stack,
                                             .stack_words = xRTOS_CONFIG_TIMER_TASK_STACK_WORDS,
                                             .name = "TimerDaemon"};

    ret = xRTOS_Task_Create(&kernel_ctx->timer_task_ctx, &timer_task_config);
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }
#endif

    xRTOS_LOG(xRETURN_xRTOS_OK, "kernel initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Kernel_Start(void)
{
    if (!is_kernel_ready())
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    // The idle task must be explicitly registered at xRTOS_IDLE_PRIORITY.
    if (g_kernel->task_by_priority[xRTOS_IDLE_PRIORITY] == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    xRETURN_t ret = xRTOS_Scheduler_Select_Next();
    if (ret != xRETURN_xRTOS_OK)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t first_task_id = g_kernel->scheduler.next_task_id;
    xRTOS_Task_Context_t *task_ctx = g_kernel->task_table[first_task_id];
    xASSERT(task_ctx != NULL, "first ready task is NULL");

    g_kernel->scheduler.current_task_id = first_task_id;
    g_kernel->scheduler.current_priority = task_ctx->effective_priority;
    xrtos_scheduler_ready_remove(g_kernel, first_task_id);
    task_ctx->state = xRTOS_TASK_STATE_RUNNING;
    g_kernel->scheduler.is_started = true;

    xRTOS_TRACE_E1(g_kernel, xRTOS_TRACE_CODE_KERNEL_START, first_task_id);

    xRTOS_LOG(xRETURN_xRTOS_OK, "scheduler starting");

    // Does not return on real hardware; the port restores the CPU context.
    // Host stubs may return so unit tests can validate the start path.
    g_kernel->port_ops->start_first_task(task_ctx);

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Task_Create(xRTOS_Task_Context_t *task_ctx, const xRTOS_Task_Config_t *task_config)
{
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (task_config == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (!is_kernel_ready())
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    xRETURN_t ret = validate_task_config(task_config);
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }

    uint32_t saved_state = 0U;
    bool lock_scheduler = g_kernel->scheduler.is_started;
    if (lock_scheduler)
    {
        saved_state = xRTOS_Scheduler_Lock();
    }

    if (g_kernel->task_table[task_config->task_id] != NULL)
    {
        if (lock_scheduler)
        {
            xRTOS_Scheduler_Unlock(saved_state);
        }
        return xRETURN_xERR_xRTOS_TASK_LIMIT;
    }

    (void)memset(task_ctx, 0, sizeof(xRTOS_Task_Context_t));

    task_ctx->task_id = task_config->task_id;
    task_ctx->base_priority = task_config->priority;
    task_ctx->effective_priority = task_config->priority;
    task_ctx->state = xRTOS_TASK_STATE_READY;
    task_ctx->stack_mem = task_config->stack_mem;
    task_ctx->stack_words = task_config->stack_words;
    task_ctx->name = task_config->name;
    task_ctx->block_status = xRETURN_xRTOS_OK;

    // Optionally fill the whole stack with a sentinel pattern so that
    // xRTOS_Task_Get_Stack_Watermark can measure the minimum free depth.
#if xRTOS_CONFIG_STACK_WATERMARK_ENABLE
    for (uint32_t i = 0U; i < task_config->stack_words; i++)
    {
        task_config->stack_mem[i] = xRTOS_STACK_FILL_PATTERN;
    }
#endif

    // Canary written to stack_mem[0] for overflow detection.
    task_ctx->stack_mem[0U] = xRTOS_STACK_CANARY;

    // Port sets up the initial stack frame and points stack_top at it.
    g_kernel->port_ops->init_task_stack(task_ctx, task_config->entry, task_config->entry_arg);

    g_kernel->task_table[task_config->task_id] = task_ctx;
    if (g_kernel->task_by_priority[task_config->priority] == NULL)
    {
        g_kernel->task_by_priority[task_config->priority] = task_ctx;
    }

    xrtos_scheduler_ready_add(g_kernel, task_config->task_id);

    xRTOS_TRACE_E2(g_kernel, xRTOS_TRACE_CODE_TASK_CREATE, task_ctx->task_id, task_ctx->effective_priority);
    xRTOS_TRACE_NAME(g_kernel, xRTOS_TRACE_OBJ_TASK, task_config->task_id, task_config->name);

    xRTOS_LOG(xRETURN_xRTOS_OK, "task registered");

    bool should_preempt = false;
    if (lock_scheduler)
    {
        if ((g_kernel->scheduler.current_task_id != xRTOS_INVALID_TASK_ID) &&
            (task_ctx->effective_priority < g_kernel->scheduler.current_priority))
        {
            g_kernel->scheduler.is_schedule_pending = true;
            should_preempt = true;
        }
        xRTOS_Scheduler_Unlock(saved_state);
    }

    if (should_preempt && !g_kernel->port_ops->is_in_isr())
    {
        g_kernel->port_ops->yield();
    }

    return xRETURN_xRTOS_OK;
}

void xRTOS_Task_Exit(void)
{
    xASSERT(g_kernel != NULL, "kernel not initialized");
    xASSERT(g_kernel->is_initialized, "kernel not initialized");

    uint32_t task_id = g_kernel->scheduler.current_task_id;

    xASSERT(task_id < xRTOS_MAX_TASKS, "current_task_id out of range");

    xRTOS_Task_Context_t *task_ctx = g_kernel->task_table[task_id];

    xASSERT(task_ctx != NULL, "current task is NULL");
    xASSERT(task_ctx->held_mutex_head == NULL, "task exited with held mutexes");

    task_ctx->state = xRTOS_TASK_STATE_TERMINATED;
    xrtos_scheduler_ready_remove(g_kernel, task_id);

    xRTOS_TRACE_E1(g_kernel, xRTOS_TRACE_CODE_TASK_EXIT, task_id);

    // Clear registry slots so they can be immediately reused.
    uint32_t priority = task_ctx->base_priority;
    g_kernel->task_table[task_id] = NULL;

    g_kernel->task_by_priority[priority] = task_find_first_at_base_priority(priority, NULL);

    g_kernel->scheduler.is_schedule_pending = true;
    g_kernel->port_ops->yield();

    // Unreachable: the terminated task is never rescheduled after yield.
    for (;;)
    {
    }
}

#if xTRACE_ENABLE
xRETURN_t xRTOS_Kernel_Trace_Init(struct xTRACE_Context_t *trace_ctx)
{
    if (trace_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if (!is_kernel_ready())
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    g_kernel->trace_ctx = trace_ctx;
    return xRETURN_xRTOS_OK;
}
#endif

void xRTOS_Task_Yield(void)
{
    xASSERT(g_kernel != NULL, "kernel not initialized");
    if (xrtos_blocking_call_is_from_isr(g_kernel))
    {
        return;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();
    g_kernel->scheduler.is_schedule_pending = true;
    xRTOS_Scheduler_Unlock(saved_state);

    g_kernel->port_ops->yield();
}

xRETURN_t xRTOS_Task_Set_Priority(uint32_t task_id, uint32_t new_priority)
{
    if ((g_kernel == NULL) || (!g_kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if ((task_id >= xRTOS_MAX_TASKS) || (new_priority >= xRTOS_MAX_PRIORITIES) || (new_priority == xRTOS_IDLE_PRIORITY))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    xRTOS_Task_Context_t *task_ctx = g_kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    uint32_t old_base = task_ctx->base_priority;

    if (old_base == new_priority)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xRTOS_OK;
    }

    // Update task_by_priority: clear old slot if this was the first task there.
    if (g_kernel->task_by_priority[old_base] == task_ctx)
    {
        g_kernel->task_by_priority[old_base] = task_find_first_at_base_priority(old_base, task_ctx);
    }

    // Set new slot if nothing is registered there yet.
    if (g_kernel->task_by_priority[new_priority] == NULL)
    {
        g_kernel->task_by_priority[new_priority] = task_ctx;
    }

    task_ctx->base_priority = new_priority;

    // Compute the new effective priority. A PI boost (effective < old_base) is kept
    // only when the boost still outranks the new base (effective < new_priority).
    // If the new base beats or matches the boost, the base takes over.
    // Lower number = higher priority throughout.
    bool is_boosted = (task_ctx->effective_priority < old_base);
    uint32_t new_effective = (is_boosted && (task_ctx->effective_priority < new_priority)) ? task_ctx->effective_priority : new_priority;

    if (new_effective != task_ctx->effective_priority)
    {
        uint32_t old_effective = task_ctx->effective_priority;

        if (task_ctx->state == xRTOS_TASK_STATE_READY)
        {
            xrtos_scheduler_ready_remove(g_kernel, task_id);
            task_ctx->effective_priority = new_effective;
            xrtos_scheduler_ready_add(g_kernel, task_id);
        }
        else
        {
            task_ctx->effective_priority = new_effective;
        }

        xrtos_mutex_waiter_priority_changed(task_ctx, old_effective);
        xrtos_mutex_owner_recompute(task_ctx, old_effective);

        // Mirror current_priority if this is the running task.
        if (g_kernel->scheduler.current_task_id == task_id)
        {
            g_kernel->scheduler.current_priority = new_effective;
        }

        // Mirror next_priority if this is the next selected task.
        if (g_kernel->scheduler.next_task_id == task_id)
        {
            g_kernel->scheduler.next_priority = new_effective;
        }
    }

    // Determine if we need to request a context switch (preemption).
    (void)xRTOS_Scheduler_Select_Next();
    if (g_kernel->scheduler.next_priority < g_kernel->scheduler.current_priority)
    {
        g_kernel->scheduler.is_schedule_pending = true;
    }

    xRTOS_TRACE_E2(g_kernel, xRTOS_TRACE_CODE_TASK_PRIO, task_id, task_ctx->effective_priority);

    xrtos_scheduler_unlock_and_maybe_yield(g_kernel, saved_state, NULL);
    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Priority_Find_Free(uint32_t preferred_priority, xRTOS_Priority_Search_Mode_t search_mode, uint32_t *assigned_priority)
{
    if (!is_kernel_ready())
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if (assigned_priority == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    // The idle priority is reserved; the helper must not return it.
    if (preferred_priority > xRTOS_LOWEST_USER_PRIORITY)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    switch (search_mode)
    {
    case xRTOS_PRIORITY_SEARCH_MODE_EXACT:
    {
        if (g_kernel->task_by_priority[preferred_priority] == NULL)
        {
            *assigned_priority = preferred_priority;
            return xRETURN_xRTOS_OK;
        }
        return xRETURN_xERR_xRTOS_PRIORITY_IN_USE;
    }

    case xRTOS_PRIORITY_SEARCH_MODE_TOWARD_HIGHER:
    {
        // Scan from preferred_priority down to xRTOS_HIGHEST_PRIORITY (0).
        // Use a break-on-zero pattern to avoid unsigned wrap-around.
        uint32_t p = preferred_priority;
        for (;;)
        {
            if (g_kernel->task_by_priority[p] == NULL)
            {
                *assigned_priority = p;
                return xRETURN_xRTOS_OK;
            }
            if (p == xRTOS_HIGHEST_PRIORITY)
            {
                break;
            }
            p--;
        }
        return xRETURN_xERR_xRTOS_PRIORITY_IN_USE;
    }

    case xRTOS_PRIORITY_SEARCH_MODE_TOWARD_LOWER:
    {
        // Scan from preferred_priority up to xRTOS_LOWEST_USER_PRIORITY.
        for (uint32_t p = preferred_priority; p <= xRTOS_LOWEST_USER_PRIORITY; p++)
        {
            if (g_kernel->task_by_priority[p] == NULL)
            {
                *assigned_priority = p;
                return xRETURN_xRTOS_OK;
            }
        }
        return xRETURN_xERR_xRTOS_PRIORITY_IN_USE;
    }

    default:
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    }
}

xRETURN_t xRTOS_Task_Get_Stack_Watermark(uint32_t task_id, uint32_t *words_free)
{
    xASSERT(words_free != NULL, "words_free is NULL");

    if (words_free == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

#if !xRTOS_CONFIG_STACK_WATERMARK_ENABLE
    (void)task_id;
    *words_free = 0U;
    return xRETURN_xERR_xRTOS_INVALID_STATE;
#else
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();

    xASSERT(kernel != NULL, "kernel not initialised");

    if (kernel == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    if (task_id >= xRTOS_CONFIG_MAX_TASKS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    const xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    // Count words from stack_mem[1] upward that still hold the fill pattern.
    // Word [0] is the canary and is excluded from the scan.
    uint32_t count = 0U;
    for (uint32_t i = 1U; i < task_ctx->stack_words; i++)
    {
        if (task_ctx->stack_mem[i] != xRTOS_STACK_FILL_PATTERN)
        {
            break;
        }
        count++;
    }

    *words_free = count;
    return xRETURN_xRTOS_OK;
#endif
}

xRETURN_t xRTOS_Task_Get_CPU_Stats(uint32_t task_id, xRTOS_Task_CPU_Stats_t *out)
{
    xASSERT(out != NULL, "out is NULL");
    if (out == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
#if !xRTOS_CONFIG_CPU_STATS_ENABLE
    (void)task_id;
    out->ticks_running = 0U;
    out->total_ticks = 0U;
    return xRETURN_xERR_xRTOS_INVALID_STATE;
#else
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialised");
    if (kernel == NULL)
    {
        out->ticks_running = 0U;
        out->total_ticks = 0U;
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if (task_id >= xRTOS_CONFIG_MAX_TASKS)
    {
        out->ticks_running = 0U;
        out->total_ticks = 0U;
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    const xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        out->ticks_running = 0U;
        out->total_ticks = 0U;
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    out->ticks_running = task_ctx->ticks_running;
    out->total_ticks = kernel->cpu_stats_ticks_total;
    return xRETURN_xRTOS_OK;
#endif
}

void xRTOS_CPU_Stats_Reset_All(void)
{
#if xRTOS_CONFIG_CPU_STATS_ENABLE
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialised");
    if (kernel == NULL)
    {
        return;
    }
    kernel->cpu_stats_ticks_total = 0U;
    for (uint32_t i = 0U; i < xRTOS_CONFIG_MAX_TASKS; i++)
    {
        if (kernel->task_table[i] != NULL)
        {
            kernel->task_table[i]->ticks_running = 0U;
        }
    }
#endif
}

// EOF /////////////////////////////////////////////////////////////////////////////
