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

// @file xrtos_mutex.c
// @brief xRTOS mutex with chained priority inheritance.
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
#include "xrtos_mutex.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_blocking.h"
#include "xrtos_private.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void mutex_cleanup_waiter(uint32_t task_id, void *arg);
static void mutex_owner_insert(xRTOS_Task_Context_t *owner, xRTOS_Mutex_Context_t *mutex_ctx);
static void mutex_owner_remove(xRTOS_Task_Context_t *owner, xRTOS_Mutex_Context_t *mutex_ctx);
static void mutex_waiter_insert(xRTOS_Mutex_Context_t *mutex_ctx, xRTOS_Task_Context_t *task_ctx);
static void mutex_waiter_remove(xRTOS_Mutex_Context_t *mutex_ctx, xRTOS_Task_Context_t *task_ctx);
static xRETURN_t mutex_waiter_select(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t *task_id);
static uint32_t mutex_highest_waiter_priority(const xRTOS_Mutex_Context_t *mutex_ctx);
static void mutex_recompute_task_priority(xRTOS_Task_Context_t *task_ctx, uint32_t depth);
static void mutex_recompute_owner_priority(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t depth);

// PRIVATE FUNCTIONS IMPLEMENTATION ///////////////////////////////////////////////

static void mutex_owner_insert(xRTOS_Task_Context_t *owner, xRTOS_Mutex_Context_t *mutex_ctx)
{
    xASSERT(owner != NULL, "owner is NULL");
    xASSERT(mutex_ctx != NULL, "mutex_ctx is NULL");

    if ((mutex_ctx->owner_prev != NULL) || (mutex_ctx->owner_next != NULL) || (owner->held_mutex_head == mutex_ctx))
    {
        return;
    }

    mutex_ctx->owner_prev = NULL;
    mutex_ctx->owner_next = owner->held_mutex_head;
    if (owner->held_mutex_head != NULL)
    {
        owner->held_mutex_head->owner_prev = mutex_ctx;
    }
    owner->held_mutex_head = mutex_ctx;
}

static void mutex_owner_remove(xRTOS_Task_Context_t *owner, xRTOS_Mutex_Context_t *mutex_ctx)
{
    xASSERT(owner != NULL, "owner is NULL");
    xASSERT(mutex_ctx != NULL, "mutex_ctx is NULL");

    if (owner->held_mutex_head == mutex_ctx)
    {
        owner->held_mutex_head = mutex_ctx->owner_next;
    }

    if (mutex_ctx->owner_prev != NULL)
    {
        mutex_ctx->owner_prev->owner_next = mutex_ctx->owner_next;
    }
    if (mutex_ctx->owner_next != NULL)
    {
        mutex_ctx->owner_next->owner_prev = mutex_ctx->owner_prev;
    }

    mutex_ctx->owner_prev = NULL;
    mutex_ctx->owner_next = NULL;
}

static void mutex_waiter_insert(xRTOS_Mutex_Context_t *mutex_ctx, xRTOS_Task_Context_t *task_ctx)
{
    xASSERT(mutex_ctx != NULL, "mutex_ctx is NULL");
    xASSERT(task_ctx != NULL, "task_ctx is NULL");
    xASSERT(task_ctx->task_id < xRTOS_MAX_TASKS, "task_id out of range");
    xASSERT(task_ctx->effective_priority < xRTOS_MAX_PRIORITIES, "effective_priority out of range");

    if (task_ctx->mutex_waiting_on == mutex_ctx)
    {
        xRTOS_Bitmap_Set(&mutex_ctx->wait_map, task_ctx->task_id);
        return;
    }
    xASSERT(task_ctx->mutex_waiting_on == NULL, "task already waiting on a mutex");

    uint32_t priority = task_ctx->effective_priority;

    xRTOS_Task_Context_t *next = mutex_ctx->wait_head;
    while ((next != NULL) && (next->effective_priority <= priority))
    {
        next = next->mutex_wait_next;
    }

    task_ctx->mutex_wait_next = next;
    if (next != NULL)
    {
        task_ctx->mutex_wait_prev = next->mutex_wait_prev;
        next->mutex_wait_prev = task_ctx;
    }
    else
    {
        task_ctx->mutex_wait_prev = mutex_ctx->wait_tail;
        mutex_ctx->wait_tail = task_ctx;
    }

    if (task_ctx->mutex_wait_prev != NULL)
    {
        task_ctx->mutex_wait_prev->mutex_wait_next = task_ctx;
    }
    else
    {
        mutex_ctx->wait_head = task_ctx;
    }

    task_ctx->mutex_waiting_on = mutex_ctx;

    xRTOS_Bitmap_Set(&mutex_ctx->wait_map, task_ctx->task_id);
}

static void mutex_waiter_remove(xRTOS_Mutex_Context_t *mutex_ctx, xRTOS_Task_Context_t *task_ctx)
{
    xASSERT(mutex_ctx != NULL, "mutex_ctx is NULL");
    xASSERT(task_ctx != NULL, "task_ctx is NULL");

    if (task_ctx->mutex_waiting_on != mutex_ctx)
    {
        return;
    }

    if (task_ctx->mutex_wait_prev != NULL)
    {
        task_ctx->mutex_wait_prev->mutex_wait_next = task_ctx->mutex_wait_next;
    }
    else
    {
        mutex_ctx->wait_head = task_ctx->mutex_wait_next;
    }

    if (task_ctx->mutex_wait_next != NULL)
    {
        task_ctx->mutex_wait_next->mutex_wait_prev = task_ctx->mutex_wait_prev;
    }
    else
    {
        mutex_ctx->wait_tail = task_ctx->mutex_wait_prev;
    }

    task_ctx->mutex_wait_prev = NULL;
    task_ctx->mutex_wait_next = NULL;
    task_ctx->mutex_waiting_on = NULL;

    xRTOS_Bitmap_Clear(&mutex_ctx->wait_map, task_ctx->task_id);
}

static xRETURN_t mutex_waiter_select(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t *task_id)
{
    if ((mutex_ctx == NULL) || (task_id == NULL))
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Task_Context_t *waiter = mutex_ctx->wait_head;
    if (waiter == NULL)
    {
        return xRETURN_xERR_xRTOS_NO_TASKS_READY;
    }

    *task_id = waiter->task_id;
    mutex_waiter_remove(mutex_ctx, waiter);
    return xRETURN_xRTOS_OK;
}

static uint32_t mutex_highest_waiter_priority(const xRTOS_Mutex_Context_t *mutex_ctx)
{
    xASSERT(mutex_ctx != NULL, "mutex_ctx is NULL");

    if (mutex_ctx->wait_head != NULL)
    {
        return mutex_ctx->wait_head->effective_priority;
    }
    return xRTOS_INVALID_TASK_ID;
}

static void mutex_recompute_owner_priority(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t depth)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (mutex_ctx == NULL) || (mutex_ctx->owner_task_id >= xRTOS_MAX_TASKS))
    {
        return;
    }

    xRTOS_Task_Context_t *owner = kernel->task_table[mutex_ctx->owner_task_id];
    if (owner != NULL)
    {
        mutex_recompute_task_priority(owner, depth);
    }
}

static void mutex_recompute_task_priority(xRTOS_Task_Context_t *task_ctx, uint32_t depth)
{
    if ((task_ctx == NULL) || (depth > xRTOS_MAX_TASKS))
    {
        return;
    }

    uint32_t inherited_priority = task_ctx->base_priority;
    xRTOS_Mutex_Context_t *held = task_ctx->held_mutex_head;
    while (held != NULL)
    {
        uint32_t waiter_priority = mutex_highest_waiter_priority(held);
        if (waiter_priority < inherited_priority)
        {
            inherited_priority = waiter_priority;
        }
        held = held->owner_next;
    }

    uint32_t old_priority = task_ctx->effective_priority;
    (void)xRTOS_Scheduler_Set_Effective_Priority(task_ctx->task_id, inherited_priority);

    if ((old_priority != inherited_priority) && (task_ctx->mutex_waiting_on != NULL))
    {
        mutex_recompute_owner_priority(task_ctx->mutex_waiting_on, depth + 1U);
    }
}

static void mutex_cleanup_waiter(uint32_t task_id, void *arg)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    xRTOS_Mutex_Context_t *mutex_ctx = (xRTOS_Mutex_Context_t *)arg;
    if ((kernel == NULL) || (mutex_ctx == NULL) || (task_id >= xRTOS_MAX_TASKS))
    {
        return;
    }

    xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
    if (task_ctx == NULL)
    {
        return;
    }

    mutex_waiter_remove(mutex_ctx, task_ctx);
    mutex_recompute_owner_priority(mutex_ctx, 0U);
}

void xrtos_mutex_waiter_priority_changed(xRTOS_Task_Context_t *task_ctx, uint32_t old_priority)
{
    if ((task_ctx == NULL) || (task_ctx->mutex_waiting_on == NULL) || (old_priority == task_ctx->effective_priority))
    {
        return;
    }

    xRTOS_Mutex_Context_t *mutex_ctx = task_ctx->mutex_waiting_on;
    mutex_waiter_remove(mutex_ctx, task_ctx);
    mutex_waiter_insert(mutex_ctx, task_ctx);
}

void xrtos_mutex_owner_recompute(xRTOS_Task_Context_t *task_ctx, uint32_t old_priority)
{
    if ((task_ctx == NULL) || (task_ctx->mutex_waiting_on == NULL) || (old_priority == task_ctx->effective_priority))
    {
        return;
    }
    mutex_recompute_owner_priority(task_ctx->mutex_waiting_on, 0U);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Mutex_Init(xRTOS_Mutex_Context_t *mutex_ctx, const char *name)
{
    if (mutex_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    mutex_ctx->owner_task_id = xRTOS_INVALID_TASK_ID;
    xRTOS_Bitmap_Reset(&mutex_ctx->wait_map);
    mutex_ctx->wait_head = NULL;
    mutex_ctx->wait_tail = NULL;
    mutex_ctx->owner_prev = NULL;
    mutex_ctx->owner_next = NULL;
    mutex_ctx->name = name;

    if (name != NULL)
    {
        xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
        xRTOS_TRACE_NAME(kernel, xRTOS_TRACE_OBJ_MUTEX, 0U, name);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "mutex initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Mutex_Lock(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t timeout_ticks)
{
    if (mutex_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

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

    xRTOS_Task_Context_t *caller = xRTOS_Scheduler_Current_Task();
    if (caller == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    uint32_t caller_task_id = caller->task_id;

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Fast path: mutex is free.
    if (mutex_ctx->owner_task_id == xRTOS_INVALID_TASK_ID)
    {
        mutex_ctx->owner_task_id = caller->task_id;
        mutex_owner_insert(caller, mutex_ctx);

        xRTOS_Scheduler_Unlock(saved_state);
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_MUTEX_LOCK, caller_task_id);
        xRTOS_LOG(xRETURN_xRTOS_OK, "mutex locked (fast path)");
        return xRETURN_xRTOS_OK;
    }

    // Mutex is held. Recursive locking is not supported.
    if (mutex_ctx->owner_task_id == caller_task_id)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    // Mutex is held by another task.
    if (timeout_ticks == xRTOS_NO_WAIT)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_WOULD_BLOCK;
    }

    xRTOS_Task_Context_t *owner = NULL;
    if (mutex_ctx->owner_task_id < xRTOS_MAX_TASKS)
    {
        owner = kernel->task_table[mutex_ctx->owner_task_id];
    }
    if (owner == NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    // Block the caller on this mutex's wait map.
    xrtos_blocking_prepare(kernel, caller, timeout_ticks, mutex_cleanup_waiter, mutex_ctx);
    mutex_waiter_insert(mutex_ctx, caller);
    mutex_recompute_task_priority(owner, 0U);

    xRETURN_t ret = xRTOS_Scheduler_Block_Current(&mutex_ctx->wait_map);
    if (ret != xRETURN_xRTOS_OK)
    {
        mutex_waiter_remove(mutex_ctx, caller);
        xrtos_blocking_cancel(kernel, caller);
        mutex_recompute_task_priority(owner, 0U);
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    xRTOS_LOG(xRETURN_xRTOS_OK, "mutex locked (blocking path)");

    return caller->block_status;
}

xRETURN_t xRTOS_Mutex_Unlock(xRTOS_Mutex_Context_t *mutex_ctx)
{
    if (mutex_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    if (!kernel->scheduler.is_started)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    if (mutex_ctx->owner_task_id >= xRTOS_MAX_TASKS)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    // Verify ownership: the calling task must be the current owner.
    xRTOS_Task_Context_t *owner = kernel->task_table[mutex_ctx->owner_task_id];
    if (owner == NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    xRTOS_Task_Context_t *caller = xRTOS_Scheduler_Current_Task();
    if (caller == NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    if (caller->task_id != mutex_ctx->owner_task_id)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    mutex_owner_remove(owner, mutex_ctx);

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_MUTEX_UNLOCK, caller->task_id);

    mutex_ctx->owner_task_id = xRTOS_INVALID_TASK_ID;

    // Hand ownership to the highest-priority waiter, or release the mutex.
    if (mutex_ctx->wait_head != NULL)
    {
        uint32_t waiter_task_id = 0U;
        if (mutex_waiter_select(mutex_ctx, &waiter_task_id) == xRETURN_xRTOS_OK)
        {
            xRTOS_Task_Context_t *waiter = kernel->task_table[waiter_task_id];
            mutex_ctx->owner_task_id = waiter_task_id;
            if (waiter != NULL)
            {
                mutex_owner_insert(waiter, mutex_ctx);
                (void)xRTOS_Scheduler_Unblock(waiter_task_id, xRETURN_xRTOS_OK);
                mutex_recompute_task_priority(waiter, 0U);
            }
            xRTOS_TRACE_E2(kernel, xRTOS_TRACE_CODE_MUTEX_HANDOFF, caller->task_id, waiter_task_id);
        }
    }
    mutex_recompute_task_priority(owner, 0U);
    bool pending = kernel->scheduler.is_schedule_pending;
    xRTOS_Scheduler_Unlock(saved_state);
    if (pending && !kernel->port_ops->is_in_isr())
    {
        kernel->port_ops->yield();
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "mutex unlocked");

    return xRETURN_xRTOS_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
