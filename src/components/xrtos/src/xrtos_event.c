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

// @file xrtos_event.c
// @brief xRTOS event flags: multicast signalling with WAIT_ANY / WAIT_ALL.
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
#include "xrtos_event.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_blocking.h"
#include "xrtos_private.h"
#include "xrtos_trace.h"
#include "xrtos_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////
#define xRTOS_EVENT_WAIT_MODE_MASK (xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_WAIT_ALL)
#define xRTOS_EVENT_OPTION_MASK    (xRTOS_EVENT_WAIT_MODE_MASK | xRTOS_EVENT_CLEAR_ON_EXIT)

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static bool is_condition_met(uint32_t current_flags, uint32_t wait_mask, uint32_t options);
static bool event_wait_options_are_valid(uint32_t options);
static xRETURN_t event_set_core(xRTOS_Event_Context_t *event_ctx, uint32_t flags, bool *should_yield);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////

static bool is_condition_met(uint32_t current_flags, uint32_t wait_mask, uint32_t options)
{
    if ((options & xRTOS_EVENT_WAIT_ALL) != 0U)
    {
        return (current_flags & wait_mask) == wait_mask;
    }
    return (current_flags & wait_mask) != 0U;
}

static bool event_wait_options_are_valid(uint32_t options)
{
    uint32_t wait_mode = options & xRTOS_EVENT_WAIT_MODE_MASK;
    bool has_known_mode = (wait_mode == xRTOS_EVENT_WAIT_ANY) || (wait_mode == xRTOS_EVENT_WAIT_ALL);
    bool has_no_unknown_options = (options & ~xRTOS_EVENT_OPTION_MASK) == 0U;

    return has_known_mode && has_no_unknown_options;
}

// Core set logic shared by xRTOS_Event_Set and xRTOS_Event_Set_From_ISR.
static xRETURN_t event_set_core(xRTOS_Event_Context_t *event_ctx, uint32_t flags, bool *should_yield)
{
    if (event_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    event_ctx->flags |= flags;
    uint32_t flags_snapshot = event_ctx->flags;
    uint32_t clear_mask = 0U;
    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_EVENT_SET, event_ctx->flags);

    xRTOS_Bitmap_t scan = event_ctx->wait_map;
    uint32_t task_id = 0U;

    while (xRTOS_Bitmap_Find_First_Set(&scan, &task_id) == xRETURN_xRTOS_OK)
    {
        xRTOS_Bitmap_Clear(&scan, task_id);

        if (task_id >= xRTOS_MAX_TASKS)
        {
            continue;
        }

        xRTOS_Task_Context_t *task_ctx = kernel->task_table[task_id];
        if (task_ctx == NULL)
        {
            continue;
        }

        uint32_t mask = task_ctx->block_payload.event.wait_mask;
        uint32_t options = task_ctx->block_payload.event.wait_options;

        if (!is_condition_met(flags_snapshot, mask, options))
        {
            continue;
        }

        // Condition satisfied against the original flag snapshot.
        uint32_t matched = flags_snapshot & mask;
        if ((options & xRTOS_EVENT_CLEAR_ON_EXIT) != 0U)
        {
            clear_mask |= matched;
        }

        task_ctx->block_payload.event.wait_mask = matched;

        (void)xRTOS_Scheduler_Unblock(task_id, xRETURN_xRTOS_OK);
    }

    event_ctx->flags = flags_snapshot & ~clear_mask;

    xRTOS_LOG(xRETURN_xRTOS_OK, "event flags set");

    xrtos_scheduler_unlock_and_maybe_yield(kernel, saved_state, should_yield);

    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Event_Init(xRTOS_Event_Context_t *event_ctx, const char *name)
{
    if (event_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    event_ctx->flags = 0U;

    xRTOS_Bitmap_Reset(&event_ctx->wait_map);
    event_ctx->name = name;

    if (name != NULL)
    {
        xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
        xRTOS_TRACE_NAME(kernel, xRTOS_TRACE_OBJ_EVENT, 0U, name);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "event initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Event_Clear(xRTOS_Event_Context_t *event_ctx, uint32_t flags)
{
    if (event_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }

    uint32_t saved_state = xRTOS_Scheduler_Lock();
    event_ctx->flags &= ~flags;
    xRTOS_Scheduler_Unlock(saved_state);

    xRTOS_LOG(xRETURN_xRTOS_OK, "event flags cleared");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Event_Set(xRTOS_Event_Context_t *event_ctx, uint32_t flags)
{
    return event_set_core(event_ctx, flags, NULL);
}

xRETURN_t xRTOS_Event_Set_From_ISR(xRTOS_Event_Context_t *event_ctx, uint32_t flags, bool *should_yield)
{
    xASSERT(should_yield != NULL, "should_yield is NULL");
    if (should_yield == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    return event_set_core(event_ctx, flags, should_yield);
}

xRETURN_t
xRTOS_Event_Wait(xRTOS_Event_Context_t *event_ctx, uint32_t flags, uint32_t options, uint32_t timeout_ticks, uint32_t *matched_flags)
{
    if (event_ctx == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if ((flags == 0U) || (!event_wait_options_are_valid(options)))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
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

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Fast path: condition already satisfied.
    if (is_condition_met(event_ctx->flags, flags, options))
    {
        uint32_t matched = event_ctx->flags & flags;
        if ((options & xRTOS_EVENT_CLEAR_ON_EXIT) != 0U)
        {
            event_ctx->flags &= ~matched;
        }
        if (matched_flags != NULL)
        {
            *matched_flags = matched;
        }
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_EVENT_WAIT, matched);
        xRTOS_LOG(xRETURN_xRTOS_OK, "event wait satisfied (fast path)");
        xRTOS_Scheduler_Unlock(saved_state);
        return xRETURN_xRTOS_OK;
    }

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
    xrtos_blocking_prepare(kernel, task_ctx, timeout_ticks, xrtos_blocking_payload_cleanup, task_ctx);
    task_ctx->block_payload.event.wait_mask = flags;
    task_ctx->block_payload.event.wait_options = options;

    xRETURN_t block_ret = xRTOS_Scheduler_Block_Current(&event_ctx->wait_map);
    if (block_ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return block_ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    if (task_ctx->block_status != xRETURN_xRTOS_OK)
    {
        xRTOS_Task_Block_Payload_Reset(&task_ctx->block_payload);
        return task_ctx->block_status;
    }

    if (task_ctx->state != xRTOS_TASK_STATE_BLOCKED)
    {
        if (matched_flags != NULL)
        {
            *matched_flags = task_ctx->block_payload.event.wait_mask;
        }
        xRTOS_Task_Block_Payload_Reset(&task_ctx->block_payload);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "event wait satisfied (blocking path)");

    return xRETURN_xRTOS_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
