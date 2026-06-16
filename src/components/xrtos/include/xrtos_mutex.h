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

// @file xrtos_mutex.h
// @brief xRTOS mutex with priority inheritance.
//
// Mutexes differ from binary semaphores in three ways:
//   1. Ownership: only the locking task may unlock.
//   2. Priority inheritance: if a higher-priority contender blocks, the owner
//      is temporarily elevated to the highest-priority blocked contender across
//      all mutexes it owns. If the owner is itself blocked on another mutex,
//      the inherited priority propagates through that chain.
//   3. No ISR give: mutexes are task-only.
//
// Recursive locking is NOT supported.  A task that calls xRTOS_Mutex_Lock on
// a mutex it already owns gets xRETURN_xERR_xRTOS_INVALID_STATE.
//
// Usage:
//
//   xRTOS_Mutex_Context_t mtx;
//   xRTOS_Mutex_Init(&mtx, "MyMtx");
//
//   // Acquire (block up to 100 ticks).
//   xRETURN_t ret = xRTOS_Mutex_Lock(&mtx, 100U);
//
//   // Release from the same task that locked.
//   xRTOS_Mutex_Unlock(&mtx);
//

#ifndef XRTOS_MUTEX_H
#define XRTOS_MUTEX_H

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

    struct xRTOS_Task_Context_t;

    typedef struct xRTOS_Mutex_Context_t
    {
        uint32_t owner_task_id;  // xRTOS_INVALID_TASK_ID when unlocked; task_id of owner otherwise
        xRTOS_Bitmap_t wait_map; // task-id-indexed map of blocked contenders

        // Priority-sorted intrusive waiter list. The head is the highest-priority
        // waiter, and equal-priority waiters remain FIFO.
        struct xRTOS_Task_Context_t *wait_head;
        struct xRTOS_Task_Context_t *wait_tail;

        struct xRTOS_Mutex_Context_t *owner_prev;
        struct xRTOS_Mutex_Context_t *owner_next;
        const char *name; // Optional display name (NULL = unnamed). Set after Init.

    } xRTOS_Mutex_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize mutex to unlocked state.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if mutex_ctx is NULL.
    xRETURN_t xRTOS_Mutex_Init(xRTOS_Mutex_Context_t *mutex_ctx, const char *name);

    // Acquire the mutex.
    // If unlocked: take ownership and return OK immediately.
    // If already owned by the caller: return INVALID_STATE.
    // If locked and timeout_ticks == xRTOS_NO_WAIT: return WOULD_BLOCK.
    // Otherwise: apply priority inheritance to the owner if needed, then block
    //   the calling task until the mutex is released or the timeout fires.
    // Must be called from task context only. Returns INVALID_STATE if the port
    // reports ISR context.
    xRETURN_t xRTOS_Mutex_Lock(xRTOS_Mutex_Context_t *mutex_ctx, uint32_t timeout_ticks);

    // Release the mutex.  Must be called by the task that locked it.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the caller is not the owner.
    // Recomputes the old owner's inherited priority across its remaining held
    //   mutexes, then wakes the highest-priority waiter (which becomes the new
    //   owner) or marks the mutex as unlocked if there are no waiters.
    xRETURN_t xRTOS_Mutex_Unlock(xRTOS_Mutex_Context_t *mutex_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_MUTEX_H
// EOF /////////////////////////////////////////////////////////////////////////////
