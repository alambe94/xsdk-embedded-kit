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

// @file xrtos_core.h
// @brief xRTOS kernel context, lifecycle APIs, and priority search helper.
//
// Global kernel instance policy:
//   xRTOS supports exactly one active kernel instance.  The application owns the
//   xRTOS_Kernel_Context_t storage (typically a file-scope static).
//   xRTOS_Kernel_Init registers that instance; all subsequent public APIs operate
//   on it without requiring a kernel_ctx parameter.
//
// Priority search:
//   xRTOS_Priority_Find_Free reports a free priority slot. It does not reserve it
//   or create a task. The caller must pass the result into xRTOS_Task_Create.
//

#ifndef XRTOS_CORE_H
#define XRTOS_CORE_H

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
#include "xrtos_defs.h"
#include "xrtos_port.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_trace.h"
#include "xrtos_sem.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum xRTOS_Priority_Search_Mode_t
    {
        xRTOS_PRIORITY_SEARCH_MODE_EXACT = 0U,         // Succeed only if preferred_priority is free.
        xRTOS_PRIORITY_SEARCH_MODE_TOWARD_HIGHER = 1U, // Search toward priority 0 (higher priority).
        xRTOS_PRIORITY_SEARCH_MODE_TOWARD_LOWER = 2U,  // Search toward xRTOS_LOWEST_USER_PRIORITY.
    } xRTOS_Priority_Search_Mode_t;

    // Forward declaration: xRTOS_Timer_Context_t is defined in xrtos_timer.h (Phase 16).
    struct xRTOS_Timer_Context_t;

    typedef struct xRTOS_Kernel_Context_t
    {
        xRTOS_Scheduler_Context_t scheduler;

        xRTOS_Task_Context_t *task_table[xRTOS_MAX_TASKS];            // Indexed by task_id.
        xRTOS_Task_Context_t *task_by_priority[xRTOS_MAX_PRIORITIES]; // Indexed by base priority.

        const xRTOS_Port_Ops_t *port_ops;

        uint32_t tick_count;
        xRTOS_Bitmap_t timeout_map;     // Task-id-indexed map of tasks in a timed wait.
        xRTOS_Bitmap_t notify_wait_map; // Task-id-indexed map of tasks blocked in Notify_Wait.

        struct xRTOS_Timer_Context_t *timer_table[xRTOS_MAX_TIMERS];
        xRTOS_Bitmap_t active_timers_map;

#if (xRTOS_MAX_TIMERS > 0U) && (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
        xRTOS_Task_Context_t timer_task_ctx;
        uint32_t timer_task_stack[xRTOS_CONFIG_TIMER_TASK_STACK_WORDS];
        xRTOS_Sem_Context_t timer_sem;
        uint32_t next_timer_expiry_tick; // Minimum expiry of all active timers; maintained by Timer_Start and the daemon.
#endif

        bool is_initialized;

#if xTRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx; // Optional; NULL until xRTOS_Kernel_Trace_Init is called.
#endif

#if xRTOS_CONFIG_CPU_STATS_ENABLE
        uint32_t cpu_stats_ticks_total; // Total ticks since last xRTOS_CPU_Stats_Reset_All().
#endif

    } xRTOS_Kernel_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Bootstrap: binds kernel_ctx as the global kernel instance and sets port ops.
    // This is the only public xRTOS API that accepts a kernel_ctx parameter.
    // All other APIs operate on the registered global instance.
    // Returns INVALID_STATE if asked to replace a started global instance.
    xRETURN_t xRTOS_Kernel_Init(xRTOS_Kernel_Context_t *kernel_ctx, const xRTOS_Port_Ops_t *port_ops);

    // Attach an initialised xTRACE_Context_t to the kernel for runtime event recording.
    // Must be called after xRTOS_Kernel_Init and before xRTOS_Kernel_Start.
    // trace_ctx must remain valid for the lifetime of the kernel session.
    // When xTRACE_ENABLE is 0 this function compiles to a static inline no-op.
#if xTRACE_ENABLE
    xRETURN_t xRTOS_Kernel_Trace_Init(struct xTRACE_Context_t *trace_ctx);
#else
// Forward declaration so the disabled signature matches the enabled one.
struct xTRACE_Context_t;
static inline xRETURN_t xRTOS_Kernel_Trace_Init(struct xTRACE_Context_t *trace_ctx)
{
    (void)trace_ctx;
    return xRETURN_xRTOS_OK;
}
#endif

    // Validates that the idle task is registered, then calls port_ops->start_first_task.
    // Does not return on real hardware. Host test ports may return xRETURN_xRTOS_OK.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the kernel is not initialized
    // or the idle task is absent.
    xRETURN_t xRTOS_Kernel_Start(void);

    // Reports a free priority without reserving it or creating a task.
    // The helper never returns xRTOS_IDLE_PRIORITY; that slot is reserved.
    xRETURN_t xRTOS_Priority_Find_Free(uint32_t preferred_priority, xRTOS_Priority_Search_Mode_t search_mode, uint32_t *assigned_priority);

    // Entry function for the software timer daemon task.
    void xrtos_timer_daemon_entry(void *arg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
