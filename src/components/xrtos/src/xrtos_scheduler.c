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

// @file xrtos_scheduler.c
// @brief xRTOS bitmap scheduler: next-task selection, blocking, unblocking,
//        context-switch commit, and interrupt-lock helpers.
//
// MISRA C:2012 Rule 15.5 deviation: multiple return points are used throughout
// for early exit on invalid arguments. This is an explicit project-level
// deviation documented in xsdk_style_guide.md.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_config.h"
#include "xrtos_core.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"
#include "xrtos_private.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRTOS_Task_Context_t *scheduler_task_by_current(const xRTOS_Kernel_Context_t *kernel);
static void scheduler_ready_list_insert_tail(xRTOS_Kernel_Context_t *kernel, xRTOS_Task_Context_t *task_ctx);
static void scheduler_ready_list_remove_at_priority(xRTOS_Kernel_Context_t *kernel, xRTOS_Task_Context_t *task_ctx, uint32_t priority);

// PRIVATE FUNCTIONS IMPLEMENTATION ///////////////////////////////////////////////

// Simplified to a direct table lookup. The previous 3-step fallback was
// incorrect under PI: task_by_priority is base-priority indexed but
// current_priority is effective-priority, so the final fallback could return
// the wrong task when inheritance is active.
static xRTOS_Task_Context_t *scheduler_task_by_current(const xRTOS_Kernel_Context_t *kernel)
{
    if (kernel == NULL)
    {
        return NULL;
    }

    uint32_t task_id = kernel->scheduler.current_task_id;
    if (task_id < xRTOS_MAX_TASKS)
    {
        return kernel->task_table[task_id];
    }

    return NULL;
}

static void scheduler_ready_list_insert_tail(xRTOS_Kernel_Context_t *kernel, xRTOS_Task_Context_t *task_ctx)
{
    xASSERT(kernel != NULL, "kernel is NULL");
    xASSERT(task_ctx != NULL, "task_ctx is NULL");
    xASSERT(task_ctx->task_id < xRTOS_MAX_TASKS, "task_id out of range");
    xASSERT(task_ctx->effective_priority < xRTOS_MAX_PRIORITIES, "effective_priority out of range");

    uint32_t priority = task_ctx->effective_priority;
    if (xRTOS_Bitmap_Is_Set(&kernel->scheduler.ready_map, task_ctx->task_id))
    {
        xRTOS_Bitmap_Set(&kernel->scheduler.ready_priority_map, priority);
        return;
    }
    xASSERT(task_ctx->ready_prev == NULL, "task ready_prev is stale");
    xASSERT(task_ctx->ready_next == NULL, "task ready_next is stale");

    task_ctx->ready_prev = kernel->scheduler.ready_tail[priority];
    task_ctx->ready_next = NULL;

    if (kernel->scheduler.ready_tail[priority] != NULL)
    {
        kernel->scheduler.ready_tail[priority]->ready_next = task_ctx;
    }
    else
    {
        kernel->scheduler.ready_head[priority] = task_ctx;
    }

    kernel->scheduler.ready_tail[priority] = task_ctx;
    xRTOS_Bitmap_Set(&kernel->scheduler.ready_map, task_ctx->task_id);
    xRTOS_Bitmap_Set(&kernel->scheduler.ready_priority_map, priority);
}

static void scheduler_ready_list_remove_at_priority(xRTOS_Kernel_Context_t *kernel, xRTOS_Task_Context_t *task_ctx, uint32_t priority)
{
    xASSERT(kernel != NULL, "kernel is NULL");
    xASSERT(task_ctx != NULL, "task_ctx is NULL");
    xASSERT(priority < xRTOS_MAX_PRIORITIES, "priority out of range");

    if (!xRTOS_Bitmap_Is_Set(&kernel->scheduler.ready_map, task_ctx->task_id))
    {
        return;
    }

    if (task_ctx->ready_prev != NULL)
    {
        task_ctx->ready_prev->ready_next = task_ctx->ready_next;
    }
    else
    {
        kernel->scheduler.ready_head[priority] = task_ctx->ready_next;
    }

    if (task_ctx->ready_next != NULL)
    {
        task_ctx->ready_next->ready_prev = task_ctx->ready_prev;
    }
    else
    {
        kernel->scheduler.ready_tail[priority] = task_ctx->ready_prev;
    }

    task_ctx->ready_prev = NULL;
    task_ctx->ready_next = NULL;
    xRTOS_Bitmap_Clear(&kernel->scheduler.ready_map, task_ctx->task_id);

    if (kernel->scheduler.ready_head[priority] == NULL)
    {
        xRTOS_Bitmap_Clear(&kernel->scheduler.ready_priority_map, priority);
    }
}

// Kernel passed by caller - eliminates xrtos_kernel_get() overhead on the hot
// path. Return type is void since all callers already cast the old result to void.
void xrtos_scheduler_ready_add(xRTOS_Kernel_Context_t *kernel, uint32_t task_id)
{
    xASSERT(kernel != NULL, "kernel is NULL");
    xASSERT(task_id < xRTOS_MAX_TASKS, "task_id out of range");

    xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    xASSERT(task_ctx != NULL, "task_ctx is NULL");

    scheduler_ready_list_insert_tail(kernel, task_ctx);
}

void xrtos_scheduler_ready_remove(xRTOS_Kernel_Context_t *kernel, uint32_t task_id)
{
    xASSERT(kernel != NULL, "kernel is NULL");
    xASSERT(task_id < xRTOS_MAX_TASKS, "task_id out of range");

    xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    xASSERT(task_ctx != NULL, "task_ctx is NULL");

    scheduler_ready_list_remove_at_priority(kernel, task_ctx, task_ctx->effective_priority);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Scheduler_Select_Next(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t next_priority = 0U;
    xRETURN_t ret = xRTOS_Bitmap_Find_First_Set(&kernel->scheduler.ready_priority_map, &next_priority);
    if (ret != xRETURN_xRTOS_OK)
    {
        return xRETURN_xERR_xRTOS_NO_TASKS_READY;
    }

    xRTOS_Task_Context_t *next_task = kernel->scheduler.ready_head[next_priority];
    xASSERT(next_task != NULL, "next task is NULL");

    kernel->scheduler.next_task_id = next_task->task_id;
    kernel->scheduler.next_priority = next_task->effective_priority;

    xRTOS_LOG(xRETURN_xRTOS_OK, "next selected");

    return xRETURN_xRTOS_OK;
}

xRTOS_Task_Context_t *xRTOS_Scheduler_Current_Task(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    return scheduler_task_by_current(kernel);
}

xRETURN_t xRTOS_Scheduler_Block_Current(xRTOS_Bitmap_t *wait_map_ptr)
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

    xRTOS_Task_Context_t *task_ctx = scheduler_task_by_current(kernel);
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    uint32_t task_id = task_ctx->task_id;

    xrtos_scheduler_ready_remove(kernel, task_id);
    xRTOS_Bitmap_Set(&kernel->scheduler.blocked_map, task_id);
    if (wait_map_ptr != NULL)
    {
        xRTOS_Bitmap_Set(wait_map_ptr, task_id);
    }

    task_ctx->state = xRTOS_TASK_STATE_BLOCKED;
    task_ctx->wait_map_ptr = wait_map_ptr;
    task_ctx->block_status = xRETURN_xRTOS_OK;

    kernel->scheduler.is_schedule_pending = true;

    xRTOS_TRACE_E2(kernel, xRTOS_TRACE_CODE_TASK_BLOCK, task_id, (uint32_t)(uintptr_t)wait_map_ptr);

    xRTOS_LOG(xRETURN_xRTOS_OK, "task blocked");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Scheduler_Unblock(uint32_t task_id, xRETURN_t block_status)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if (task_id >= xRTOS_MAX_TASKS)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    if (task_ctx->wait_map_ptr != NULL)
    {
        xRTOS_Bitmap_Clear(task_ctx->wait_map_ptr, task_id);
    }

    xRTOS_Bitmap_Clear(&kernel->timeout_map, task_id);
    xRTOS_Bitmap_Clear(&kernel->scheduler.blocked_map, task_id);
    xrtos_scheduler_ready_add(kernel, task_id);

    task_ctx->state = xRTOS_TASK_STATE_READY;
    task_ctx->block_status = block_status;
    task_ctx->wait_map_ptr = NULL;
    task_ctx->block_cleanup = NULL;
    task_ctx->block_cleanup_arg = NULL;

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TASK_READY, task_ctx->task_id);

    // current_priority is kept in sync by xRTOS_Scheduler_Switch; read directly.
    if (task_ctx->effective_priority < kernel->scheduler.current_priority)
    {
        kernel->scheduler.is_schedule_pending = true;
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "task unblocked");

    return xRETURN_xRTOS_OK;
}

void xRTOS_Scheduler_Switch(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialized");
    xASSERT(kernel->is_initialized, "kernel not initialized");
    xASSERT(kernel->scheduler.is_started, "scheduler not started");

    uint32_t prev_task_id = kernel->scheduler.current_task_id;
    uint32_t next_task_id = kernel->scheduler.next_task_id;

    xASSERT(next_task_id < xRTOS_MAX_TASKS, "next_task_id out of range");

    xRTOS_Task_Context_t *next_task = kernel->task_table[next_task_id];
    xASSERT(next_task != NULL, "next task is NULL");
    if (!xRTOS_Task_Stack_Is_Valid(next_task))
    {
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_STACK_CORRUPT, next_task_id);
        xASSERT(false, "next task stack canary corrupted");
    }

    // Transition the previous task back to READY unless it self-terminated or
    // was suspended; those states are left unchanged.
    if (prev_task_id < xRTOS_MAX_TASKS)
    {
        xRTOS_Task_Context_t *prev_task = kernel->task_table[prev_task_id];
        if (prev_task != NULL)
        {
            if (!xRTOS_Task_Stack_Is_Valid(prev_task))
            {
                xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_STACK_CORRUPT, prev_task_id);
                xASSERT(false, "previous task stack canary corrupted");
            }

            if (prev_task->state == xRTOS_TASK_STATE_RUNNING)
            {
                prev_task->state = xRTOS_TASK_STATE_READY;
                xrtos_scheduler_ready_add(kernel, prev_task_id);
                xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TASK_READY, prev_task->task_id);
            }
        }
    }

    xrtos_scheduler_ready_remove(kernel, next_task_id);
    next_task->state = xRTOS_TASK_STATE_RUNNING;
    kernel->scheduler.current_task_id = next_task_id;
    kernel->scheduler.current_priority = next_task->effective_priority;
    kernel->scheduler.is_schedule_pending = false;

    xRTOS_TRACE_E2(kernel, xRTOS_TRACE_CODE_TASK_SWITCH, prev_task_id, next_task_id);

    xRTOS_LOG(xRETURN_xRTOS_OK, "context switched");
}

xRETURN_t xRTOS_Scheduler_Unblock_From_Wait_Map(xRTOS_Bitmap_t *wait_map, uint32_t *unblocked_task_id)
{
    if (wait_map == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t task_id = 0U;
    xRETURN_t ret = xRTOS_Scheduler_Find_Highest_Task_In_Map(wait_map, &task_id);
    if (ret != xRETURN_xRTOS_OK)
    {
        return xRETURN_xERR_xRTOS_NO_TASKS_READY;
    }

    (void)xRTOS_Scheduler_Unblock(task_id, xRETURN_xRTOS_OK);

    if (unblocked_task_id != NULL)
    {
        *unblocked_task_id = task_id;
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "task unblocked from wait map");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Scheduler_Find_Highest_Task_In_Map(const xRTOS_Bitmap_t *task_map, uint32_t *task_id)
{
    if ((task_map == NULL) || (task_id == NULL))
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t best_task_id = xRTOS_INVALID_TASK_ID;
    uint32_t best_priority = xRTOS_MAX_PRIORITIES;

    // Use CTZ to walk only set bits rather than iterating all N task slots.
    for (uint32_t w = 0U; w < xRTOS_BITMAP_WORD_COUNT; w++)
    {
        uint32_t word = task_map->words[w] & xrtos_bitmap_word_mask(w);
        while (word != 0U)
        {
            uint32_t bit = xrtos_bitmap_ctz32(word);
            word &= word - 1U;
            uint32_t i = (w * xRTOS_BITMAP_WORD_BITS) + bit;
            if (i >= xRTOS_MAX_TASKS)
            {
                continue;
            }

            xRTOS_Task_Context_t *candidate = kernel->task_table[i];
            if (candidate == NULL)
            {
                continue;
            }

            if ((candidate->effective_priority < best_priority) ||
                ((candidate->effective_priority == best_priority) && (candidate->task_id < best_task_id)))
            {
                best_task_id = candidate->task_id;
                best_priority = candidate->effective_priority;
            }
        }
    }

    if (best_task_id == xRTOS_INVALID_TASK_ID)
    {
        return xRETURN_xERR_xRTOS_NO_TASKS_READY;
    }

    *task_id = best_task_id;
    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Scheduler_Set_Effective_Priority(uint32_t task_id, uint32_t effective_priority)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if ((task_id >= xRTOS_MAX_TASKS) || (effective_priority >= xRTOS_MAX_PRIORITIES))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    uint32_t old_priority = task_ctx->effective_priority;
    bool was_ready = xRTOS_Bitmap_Is_Set(&kernel->scheduler.ready_map, task_id);
    if (was_ready)
    {
        scheduler_ready_list_remove_at_priority(kernel, task_ctx, old_priority);
    }

    task_ctx->effective_priority = effective_priority;
    xrtos_mutex_waiter_priority_changed(task_ctx, old_priority);
    if (kernel->scheduler.current_task_id == task_id)
    {
        kernel->scheduler.current_priority = effective_priority;
    }
    if (kernel->scheduler.next_task_id == task_id)
    {
        kernel->scheduler.next_priority = effective_priority;
    }
    if (was_ready)
    {
        scheduler_ready_list_insert_tail(kernel, task_ctx);
    }
    if (old_priority != effective_priority)
    {
        xRTOS_TRACE_E2(kernel, xRTOS_TRACE_CODE_TASK_PRIO, task_id, effective_priority);
    }

    return xRETURN_xRTOS_OK;
}

uint32_t xRTOS_Scheduler_Lock(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialized");
    xASSERT(kernel->is_initialized, "kernel not initialized");

    return kernel->port_ops->disable_interrupts();
}

void xRTOS_Scheduler_Unlock(uint32_t saved_state)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xASSERT(kernel != NULL, "kernel not initialized");
    xASSERT(kernel->is_initialized, "kernel not initialized");

    kernel->port_ops->enable_interrupts(saved_state);
}

// EOF /////////////////////////////////////////////////////////////////////////////
