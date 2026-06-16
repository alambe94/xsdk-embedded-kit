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

// @file xrtos_notify.c
// @brief xRTOS task notification: give, ISR give, and timed wait.
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
#include "xrtos_notify.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"
#include "xrtos_blocking.h"
#include "xrtos_private.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t notify_core(uint32_t task_id, uint32_t value, bool *should_yield);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////

// Core notification logic shared by xRTOS_Task_Notify and xRTOS_Task_Notify_From_ISR.
// should_yield is NULL when called from task context (ignored).
static xRETURN_t notify_core(uint32_t task_id, uint32_t value, bool *should_yield)
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

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Latch the notification value and set the pending flag.
    task_ctx->notify_value |= value;
    task_ctx->has_notify_pending = true;

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TASK_NOTIFY, task_id);

    xRTOS_LOG(xRETURN_xRTOS_OK, "notification sent");

    uint32_t waiting_task_id = task_ctx->task_id;

    // If the task is blocked in Notify_Wait, wake it immediately.
    if ((task_ctx->state == xRTOS_TASK_STATE_BLOCKED) && xRTOS_Bitmap_Is_Set(&kernel->notify_wait_map, waiting_task_id))
    {
        // Latch the pre-clear value for the resumed waiter, then apply clear-on-exit.
        task_ctx->block_payload.notify.value = task_ctx->notify_value;
        task_ctx->notify_value &= ~task_ctx->block_payload.notify.clear_on_exit;
        task_ctx->has_notify_pending = false;

        // Transition the task to READY with OK status (sets wait_map_ptr = NULL).
        (void)xRTOS_Scheduler_Unblock(waiting_task_id, xRETURN_xRTOS_OK);

        xRTOS_LOG(xRETURN_xRTOS_OK, "blocked task woken by notification");
    }

    xrtos_scheduler_unlock_and_maybe_yield(kernel, saved_state, should_yield);

    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Task_Notify(uint32_t task_id, uint32_t value)
{
    return notify_core(task_id, value, NULL);
}

xRETURN_t xRTOS_Task_Notify_From_ISR(uint32_t task_id, uint32_t value, bool *should_yield)
{
    xASSERT(should_yield != NULL, "should_yield is NULL");
    if (should_yield == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    return notify_core(task_id, value, should_yield);
}

xRETURN_t xRTOS_Task_Notify_Wait(uint32_t clear_on_entry, uint32_t clear_on_exit, uint32_t *value, uint32_t timeout_ticks)
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

    xRTOS_Task_Context_t *task_ctx = xRTOS_Scheduler_Current_Task();
    if (task_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Apply clear-on-entry before checking pending (allows masking stale bits).
    task_ctx->notify_value &= ~clear_on_entry;

    // Fast path: notification already waiting - collect and return immediately.
    if (task_ctx->has_notify_pending)
    {
        if (value != NULL)
        {
            *value = task_ctx->notify_value;
        }
        task_ctx->notify_value &= ~clear_on_exit;
        task_ctx->has_notify_pending = false;
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xRTOS_OK;
    }

    // No pending notification; return immediately if caller does not want to block.
    if (timeout_ticks == xRTOS_NO_WAIT)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xERR_xRTOS_WOULD_BLOCK;
    }

    // Register as a notification waiter (notify_wait_map plays the role of
    // the sync object's wait_map in the blocking model).
    xrtos_blocking_prepare(kernel, task_ctx, timeout_ticks, NULL, NULL);
    task_ctx->block_payload.notify.clear_on_exit = clear_on_exit;
    task_ctx->block_payload.notify.value = 0U;

    xRETURN_t block_ret = xRTOS_Scheduler_Block_Current(&kernel->notify_wait_map);
    if (block_ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return block_ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    // Read result and optionally return the notification value.
    xRETURN_t result = task_ctx->block_status;
    if ((result == xRETURN_xRTOS_OK) && (value != NULL))
    {
        *value = task_ctx->block_payload.notify.value;
    }
    return result;
}

// EOF /////////////////////////////////////////////////////////////////////////////
