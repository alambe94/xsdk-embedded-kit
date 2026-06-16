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

// @file xrtos_sem.h
// @brief xRTOS semaphore: binary and counting, task-context and ISR-safe give.
//
// Binary semaphore:    xRTOS_Sem_Init(sem, 0U, 1U, NULL) - initially locked.
//                      xRTOS_Sem_Init(sem, 1U, 1U, NULL) - initially available.
// Counting semaphore:  xRTOS_Sem_Init(sem, N, MAX, NULL) - N tokens available.
//
// Take/Give use the blocking model: each blocked task sets its wait_map_ptr to
// &sem_ctx->wait_map so the tick ISR can disarm the timeout on expiry.
// Give wakes the highest-priority waiter via xRTOS_Scheduler_Unblock_From_Wait_Map
// and does NOT increment count when doing so - the woken task receives the token
// directly.
//
// Usage pattern:
//
//   xRTOS_Sem_Context_t sem;
//   xRTOS_Sem_Init(&sem, 0U, 1U, "MySem");             // binary, locked
//
//   // Task: wait up to 100 ticks for the semaphore.
//   xRETURN_t ret = xRTOS_Sem_Take(&sem, 100U);
//   if (ret == xRETURN_xRTOS_OK) { /* got token */ }
//   if (ret == xRETURN_xERR_xRTOS_TIMEOUT) { /* timed out */ }
//
//   // ISR: signal the semaphore.
//   bool yield;
//   xRTOS_Sem_Give_From_ISR(&sem, &yield);
//   if (yield) { port_yield(); }
//

#ifndef XRTOS_SEM_H
#define XRTOS_SEM_H

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

    typedef struct xRTOS_Sem_Context_t
    {
        uint32_t count;
        uint32_t max_count;
        xRTOS_Bitmap_t wait_map; // Task-id-indexed map of tasks blocked in Take.
        const char *name;        // Optional display name (NULL = unnamed). Set after Init.

    } xRTOS_Sem_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize a semaphore. initial_count tokens are available immediately.
    // max_count is the upper bound; initial_count must be <= max_count.
    // Binary semaphore:   max_count == 1U.
    // Counting semaphore: max_count > 1U.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if sem_ctx is NULL.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT if max_count == 0 or
    //   initial_count > max_count.
    xRETURN_t xRTOS_Sem_Init(xRTOS_Sem_Context_t *sem_ctx, uint32_t initial_count, uint32_t max_count, const char *name);

    // Acquire a token. If count > 0, decrement and return OK immediately.
    // If count == 0 and timeout_ticks == xRTOS_NO_WAIT, return WOULD_BLOCK.
    // Otherwise block the calling task until Give wakes it or the timeout fires.
    // Must be called from task context only; never from an ISR.
    // Returns xRETURN_xRTOS_OK on success (token acquired).
    // Returns xRETURN_xERR_xRTOS_WOULD_BLOCK when count == 0 and NO_WAIT.
    // Returns xRETURN_xERR_xRTOS_TIMEOUT when the timed wait expired.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE when the kernel is not started or
    // when the port reports ISR context.
    xRETURN_t xRTOS_Sem_Take(xRTOS_Sem_Context_t *sem_ctx, uint32_t timeout_ticks);

    // Release a token.
    // If a task is blocked in Take, wake the highest-priority waiter (the count
    //   is NOT incremented - the token is handed to the waiter directly).
    // If no waiter and count < max_count, increment count and return OK.
    // If no waiter and count == max_count, return xRETURN_xERR_xRTOS_RESOURCE_FULL.
    // Must be called from task context only; use Give_From_ISR from an ISR.
    xRETURN_t xRTOS_Sem_Give(xRTOS_Sem_Context_t *sem_ctx);

    // ISR-safe variant of xRTOS_Sem_Give. should_yield is set to true when the
    // woken task has strictly higher priority than the currently running task;
    // the ISR caller shall request a port yield when should_yield is true.
    // should_yield must not be NULL; NULL returns xRETURN_xERR_xRTOS_NULL_POINTER.
    xRETURN_t xRTOS_Sem_Give_From_ISR(xRTOS_Sem_Context_t *sem_ctx, bool *should_yield);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_SEM_H
// EOF /////////////////////////////////////////////////////////////////////////////
