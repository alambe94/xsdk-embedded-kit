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

// @file xrtos_queue.h
// @brief xRTOS fixed-size item queue with copy-in/copy-out semantics.
//
// Storage is caller-owned and passed to xRTOS_Queue_Init.  The queue holds at
// most item_count items of item_size bytes each.  head and tail are item
// indices (not byte offsets); the ring advances by one slot per operation.
//
// send_wait_map: task-id-indexed bitmap of tasks blocked because the queue is full.
// recv_wait_map: task-id-indexed bitmap of tasks blocked because the queue is empty.
// Blocked send item and receive buffer pointers live in the blocked task's
// block_payload field. Those pointers must remain valid while the task is blocked.
// Both satisfy the wait_map_ptr contract: when a task blocks, Block_Current sets
// the task's id bit in the map and stores the map pointer in wait_map_ptr.
//
// Usage:
//
//   static uint8_t storage[4 * sizeof(uint32_t)];
//   xRTOS_Queue_Context_t q;
//   xRTOS_Queue_Init(&q, storage, sizeof(uint32_t), 4U, "MyQueue");
//
//   // Task A: send
//   uint32_t val = 42U;
//   xRTOS_Queue_Send(&q, &val, xRTOS_WAIT_FOREVER);
//
//   // Task B: receive
//   uint32_t out;
//   xRTOS_Queue_Receive(&q, &out, xRTOS_WAIT_FOREVER);
//
//   // ISR: send
//   bool yield;
//   xRTOS_Queue_Send_From_ISR(&q, &val, &yield);
//   if (yield) { port_yield(); }
//

#ifndef XRTOS_QUEUE_H
#define XRTOS_QUEUE_H

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
#include "xrtos_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xRTOS_Queue_Context_t
    {
        uint8_t *storage;             // Caller-owned ring buffer (item_size * item_count bytes).
        uint32_t item_size;           // Bytes per item.
        uint32_t item_count;          // Maximum number of items.
        uint32_t head;                // Dequeue index (item index, not byte offset).
        uint32_t tail;                // Enqueue index (item index, not byte offset).
        uint32_t used;                // Number of items currently in the queue.
        xRTOS_Bitmap_t send_wait_map; // Task-id-indexed: tasks blocked on a full queue.
        xRTOS_Bitmap_t recv_wait_map; // Task-id-indexed: tasks blocked on an empty queue.
        const char *name;             // Optional display name (NULL = unnamed). Set after Init.

    } xRTOS_Queue_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the queue. storage must point to at least item_size * item_count
    // bytes of caller-owned memory; the queue holds a reference to it.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if queue_ctx or storage is NULL.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT if item_size or item_count is 0
    // or if item_size * item_count would overflow uint32_t indexing.
    xRETURN_t xRTOS_Queue_Init(xRTOS_Queue_Context_t *queue_ctx, void *storage, uint32_t item_size, uint32_t item_count, const char *name);

    // Copy item into the queue.
    // If the queue is not full, enqueues immediately and wakes the highest-priority
    //   blocked receiver (if any).
    // If the queue is full and timeout_ticks == xRTOS_NO_WAIT: WOULD_BLOCK.
    // Otherwise blocks the caller until space is available or the timeout fires.
    // Must be called from task context only. Returns INVALID_STATE if the port
    // reports ISR context.
    xRETURN_t xRTOS_Queue_Send(xRTOS_Queue_Context_t *queue_ctx, const void *item, uint32_t timeout_ticks);

    // Copy the oldest item out of the queue into item.
    // If the queue is not empty, dequeues immediately and wakes the highest-priority
    //   blocked sender (if any).
    // If the queue is empty and timeout_ticks == xRTOS_NO_WAIT: WOULD_BLOCK.
    // Otherwise blocks the caller until an item arrives or the timeout fires.
    // Must be called from task context only. Returns INVALID_STATE if the port
    // reports ISR context.
    xRETURN_t xRTOS_Queue_Receive(xRTOS_Queue_Context_t *queue_ctx, void *item, uint32_t timeout_ticks);

    // ISR-safe variant of xRTOS_Queue_Send. should_yield is set to true when a
    // woken receiver has strictly higher priority than the currently running task.
    // should_yield must not be NULL; NULL returns xRETURN_xERR_xRTOS_NULL_POINTER.
    xRETURN_t xRTOS_Queue_Send_From_ISR(xRTOS_Queue_Context_t *queue_ctx, const void *item, bool *should_yield);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_QUEUE_H
// EOF /////////////////////////////////////////////////////////////////////////////
