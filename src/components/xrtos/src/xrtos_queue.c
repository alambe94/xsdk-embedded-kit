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

// @file xrtos_queue.c
// @brief xRTOS fixed-size item queue with copy-in/copy-out semantics.
//
// MISRA C:2012 Rule 15.5 deviation: multiple return points are used throughout
// for early exit on invalid arguments. This is an explicit project-level
// deviation documented in xsdk_style_guide.md.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_config.h"
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_queue.h"
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

static void queue_enqueue(xRTOS_Queue_Context_t *queue_ctx, const void *item);
static void queue_dequeue(xRTOS_Queue_Context_t *queue_ctx, void *item);
static xRETURN_t queue_complete_waiting_receive(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx, const void *item);
static xRETURN_t queue_complete_waiting_send(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx);
static xRETURN_t queue_try_send_locked(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx, const void *item);
static xRETURN_t queue_send_core(xRTOS_Queue_Context_t *queue_ctx, const void *item, bool *should_yield);

// PRIVATE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////

static void queue_enqueue(xRTOS_Queue_Context_t *queue_ctx, const void *item)
{
    xASSERT(queue_ctx->tail < queue_ctx->item_count, "queue tail out of range");

    uint32_t offset = queue_ctx->tail * queue_ctx->item_size;
    uint8_t *dst = queue_ctx->storage + offset;
    (void)memcpy(dst, item, queue_ctx->item_size);
    queue_ctx->tail++;
    if (queue_ctx->tail == queue_ctx->item_count)
    {
        queue_ctx->tail = 0U;
    }
    queue_ctx->used++;
}

static void queue_dequeue(xRTOS_Queue_Context_t *queue_ctx, void *item)
{
    xASSERT(queue_ctx->head < queue_ctx->item_count, "queue head out of range");

    uint32_t offset = queue_ctx->head * queue_ctx->item_size;
    const uint8_t *src = queue_ctx->storage + offset;
    (void)memcpy(item, src, queue_ctx->item_size);
    queue_ctx->head++;
    if (queue_ctx->head == queue_ctx->item_count)
    {
        queue_ctx->head = 0U;
    }
    queue_ctx->used--;
}

static xRETURN_t queue_complete_waiting_receive(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx, const void *item)
{
    uint32_t task_id = 0U;
    xRETURN_t ret = xRTOS_Scheduler_Find_Highest_Task_In_Map(&queue_ctx->recv_wait_map, &task_id);
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }

    xRTOS_Task_Context_t *receiver = kernel->task_table[task_id];
    xASSERT(receiver != NULL, "blocked receiver task is NULL");

    void *dst = receiver->block_payload.ptr;
    xASSERT(dst != NULL, "blocked receiver buffer is NULL");

    (void)memcpy(dst, item, queue_ctx->item_size);
    xRTOS_Task_Block_Payload_Reset(&receiver->block_payload);

    (void)xRTOS_Scheduler_Unblock(task_id, xRETURN_xRTOS_OK);

    return xRETURN_xRTOS_OK;
}

static xRETURN_t queue_complete_waiting_send(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx)
{
    uint32_t task_id = 0U;
    xRETURN_t ret = xRTOS_Scheduler_Find_Highest_Task_In_Map(&queue_ctx->send_wait_map, &task_id);
    if (ret != xRETURN_xRTOS_OK)
    {
        return ret;
    }

    xRTOS_Task_Context_t *sender = kernel->task_table[task_id];
    xASSERT(sender != NULL, "blocked sender task is NULL");

    const void *src = sender->block_payload.const_ptr;
    xASSERT(src != NULL, "blocked sender item is NULL");
    xASSERT(queue_ctx->used < queue_ctx->item_count, "queue has no space for blocked sender");

    queue_enqueue(queue_ctx, src);
    xRTOS_Task_Block_Payload_Reset(&sender->block_payload);
    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_SEND, queue_ctx->used);

    (void)xRTOS_Scheduler_Unblock(task_id, xRETURN_xRTOS_OK);

    return xRETURN_xRTOS_OK;
}

// Deliver item to a blocked receiver or enqueue in the ring buffer.
// Returns RESOURCE_FULL when full with no waiting receivers.
// Caller must hold the scheduler lock.
static xRETURN_t queue_try_send_locked(xRTOS_Kernel_Context_t *kernel, xRTOS_Queue_Context_t *queue_ctx, const void *item)
{
    if (!xRTOS_Bitmap_Is_Empty(&queue_ctx->recv_wait_map))
    {
        (void)queue_complete_waiting_receive(kernel, queue_ctx, item);
        // used stays 0: item bypassed the buffer. Trace arg reflects buffer occupancy.
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_SEND, queue_ctx->used);
        return xRETURN_xRTOS_OK;
    }
    if (queue_ctx->used < queue_ctx->item_count)
    {
        queue_enqueue(queue_ctx, item);
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_SEND, queue_ctx->used);
        return xRETURN_xRTOS_OK;
    }
    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_FULL, (uint32_t)(uintptr_t)queue_ctx);
    return xRETURN_xERR_xRTOS_RESOURCE_FULL;
}

// ISR-safe send core: locks, attempts delivery/enqueue, returns RESOURCE_FULL if queue is full.
// should_yield is NULL in task context; set to is_schedule_pending in ISR context.
static xRETURN_t queue_send_core(xRTOS_Queue_Context_t *queue_ctx, const void *item, bool *should_yield)
{
    if ((queue_ctx == NULL) || (item == NULL))
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }

    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return xRETURN_xERR_xRTOS_INVALID_STATE;
    }
    uint32_t saved_state = xRTOS_Scheduler_Lock();

    xRETURN_t ret = queue_try_send_locked(kernel, queue_ctx, item);
    if (ret != xRETURN_xRTOS_OK)
    {
        xRTOS_Scheduler_Unlock(saved_state);
        return ret;
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "queue item enqueued");

    xrtos_scheduler_unlock_and_maybe_yield(kernel, saved_state, should_yield);

    return xRETURN_xRTOS_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xRTOS_Queue_Init(xRTOS_Queue_Context_t *queue_ctx, void *storage, uint32_t item_size, uint32_t item_count, const char *name)
{
    if ((queue_ctx == NULL) || (storage == NULL))
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    if ((item_size == 0U) || (item_count == 0U))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }
    if (item_count > (UINT32_MAX / item_size))
    {
        return xRETURN_xERR_xRTOS_INVALID_ARGUMENT;
    }

    queue_ctx->storage = (uint8_t *)storage;
    queue_ctx->item_size = item_size;
    queue_ctx->item_count = item_count;
    queue_ctx->head = 0U;
    queue_ctx->tail = 0U;
    queue_ctx->used = 0U;

    xRTOS_Bitmap_Reset(&queue_ctx->send_wait_map);
    xRTOS_Bitmap_Reset(&queue_ctx->recv_wait_map);
    queue_ctx->name = name;

    if (name != NULL)
    {
        xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
        xRTOS_TRACE_NAME(kernel, xRTOS_TRACE_OBJ_QUEUE, 0U, name);
    }

    xRTOS_LOG(xRETURN_xRTOS_OK, "queue initialized");

    return xRETURN_xRTOS_OK;
}

xRETURN_t xRTOS_Queue_Send(xRTOS_Queue_Context_t *queue_ctx, const void *item, uint32_t timeout_ticks)
{
    if ((queue_ctx == NULL) || (item == NULL))
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

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    xRETURN_t fast_ret = queue_try_send_locked(kernel, queue_ctx, item);
    if (fast_ret == xRETURN_xRTOS_OK)
    {
        bool pending = kernel->scheduler.is_schedule_pending;
        xRTOS_Scheduler_Unlock(saved_state);
        if (pending)
        {
            kernel->port_ops->yield();
        }
        xRTOS_LOG(xRETURN_xRTOS_OK, "queue item enqueued");
        return xRETURN_xRTOS_OK;
    }

    if (timeout_ticks == xRTOS_NO_WAIT)
    {
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_FULL, (uint32_t)(uintptr_t)queue_ctx);
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
    task_ctx->block_payload.const_ptr = item;

    xRETURN_t block_ret = xRTOS_Scheduler_Block_Current(&queue_ctx->send_wait_map);
    if (block_ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return block_ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    xRTOS_LOG(xRETURN_xRTOS_OK, "queue send (blocking path)");

    return task_ctx->block_status;
}

xRETURN_t xRTOS_Queue_Receive(xRTOS_Queue_Context_t *queue_ctx, void *item, uint32_t timeout_ticks)
{
    if ((queue_ctx == NULL) || (item == NULL))
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

    uint32_t saved_state = xRTOS_Scheduler_Lock();

    // Fast path: item available.
    if (queue_ctx->used > 0U)
    {
        queue_dequeue(queue_ctx, item);
        xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_QUEUE_RECV, queue_ctx->used);

        if (!xRTOS_Bitmap_Is_Empty(&queue_ctx->send_wait_map))
        {
            (void)queue_complete_waiting_send(kernel, queue_ctx);
        }

        bool pending = kernel->scheduler.is_schedule_pending;
        xRTOS_Scheduler_Unlock(saved_state);
        if (pending)
        {
            kernel->port_ops->yield();
        }
        xRTOS_LOG(xRETURN_xRTOS_OK, "queue receive (fast path)");
        return xRETURN_xRTOS_OK;
    }

    // Queue empty.
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
    task_ctx->block_payload.ptr = item;

    xRETURN_t block_ret = xRTOS_Scheduler_Block_Current(&queue_ctx->recv_wait_map);
    if (block_ret != xRETURN_xRTOS_OK)
    {
        xrtos_blocking_cancel(kernel, task_ctx);
        xRTOS_Scheduler_Unlock(saved_state);
        return block_ret;
    }
    xRTOS_Scheduler_Unlock(saved_state);
    kernel->port_ops->yield();

    xRTOS_LOG(xRETURN_xRTOS_OK, "queue receive (blocking path)");

    return task_ctx->block_status;
}

xRETURN_t xRTOS_Queue_Send_From_ISR(xRTOS_Queue_Context_t *queue_ctx, const void *item, bool *should_yield)
{
    xASSERT(should_yield != NULL, "should_yield is NULL");
    if (should_yield == NULL)
    {
        return xRETURN_xERR_xRTOS_NULL_POINTER;
    }
    return queue_send_core(queue_ctx, item, should_yield);
}

// EOF /////////////////////////////////////////////////////////////////////////////
