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

// @file xrtos_sem.c
// @brief xRTOS semaphore: binary and counting, task-context and ISR-safe give.
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
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_blocking.h"
#include "xrtos_private.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t sem_give_core(xRTOS_Sem_Context_t *sem_ctx, bool *should_yield);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////

// Core give logic shared by xRTOS_Sem_Give and xRTOS_Sem_Give_From_ISR.
// should_yield is NULL when called from task context (ignored).
static xRETURN_t sem_give_core(xRTOS_Sem_Context_t *sem_ctx, bool *should_yield)
{
    if (sem_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    if (!xRTOS_Bitmap_Is_Empty(&sem_ctx->wait_map))
    {
        // Wake the highest-priority waiter. The token passes directly to the
        // woken task; count is not incremented.
        (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&sem_ctx->wait_map, NULL);
    }
    else if (sem_ctx->count < sem_ctx->max_count)
    {
        sem_ctx->count++;
    }
    else
    {
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_SEM_FULL, (uint32_t)(uintptr_t)sem_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_RESOURCE_FULL;
    }
    bool pending = kernel->scheduler.is_schedule_pending;
    xRTOS_Scheduler_Unlock(saved_state);

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_SEM_GIVE, sem_ctx->count);
    xRTOS_LOG(xRETURN_xRTOS_OK, "semaphore given");

    if (should_yield != NULL)
    {
        *should_yield = pending;
    }
    else
    {
        if (pending && !kernel->port_ops->is_in_isr())
        {
            kernel->port_ops->yield();
        }
    }

    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Sem_Init(xRTOS_Sem_Context_t *sem_ctx, uint32_t initial_count, uint32_t max_count, const char *name)
{
    if (sem_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if ((max_count == 0U) || (initial_count > max_count))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    sem_ctx->count = initial_count;
    sem_ctx->max_count = max_count;
    xRTOS_Bitmap_Reset(&sem_ctx->wait_map);
    sem_ctx->name = name;

    if (name != NULL)
    {
        xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
        xRTOS_TRACE_NAME(kernel, xRTOS_TRACE_OBJ_SEM, 0U, name);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "semaphore initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Sem_Take(xRTOS_Sem_Context_t *sem_ctx, uint32_t timeout_ticks)
{
    if (sem_ctx == NULL)
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

    // Fast path: token available - decrement and return without blocking.
    uint32_t saved_state = xRTOS_Scheduler_Lock();

    if (sem_ctx->count > 0U)
    {
        sem_ctx->count--;
        xRTOS_Scheduler_Unlock(saved_state);
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_SEM_TAKE, sem_ctx->count);
        xRTOS_LOG(xRETURN_xRTOS_OK, "semaphore taken (fast path)");
        return xRETURN_xRTOS_OK;
    }

    // No token available.
    if (timeout_ticks == xRTOS_NO_WAIT)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_WOULD_BLOCK;
    }

    xRTOS_Task_Context_t *task_ctx = xRTOS_Scheduler_Current_Task();
    if (task_ctx == NULL)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    xrtos_blocking_prepare(kernel, task_ctx, timeout_ticks, NULL, NULL);

    xRETURN_t ret = xRTOS_Scheduler_Block_Current(&sem_ctx->wait_map);
    if (ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    xRTOS_LOG(xRETURN_xRTOS_OK, "semaphore taken (blocking path)");

    return task_ctx->block_status;
}

xRETURN_t xRTOS_Sem_Give(xRTOS_Sem_Context_t *sem_ctx)
{
    return sem_give_core(sem_ctx, NULL);
}

xRETURN_t xRTOS_Sem_Give_From_ISR(xRTOS_Sem_Context_t *sem_ctx, bool *should_yield)
{
    xASSERT(should_yield != NULL, "should_yield is NULL");
    if (should_yield == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    return sem_give_core(sem_ctx, should_yield);
}

// EOF /////////////////////////////////////////////////////////////////////////////
