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

// @file xrtos_event.h
// @brief xRTOS event flags: multicast signalling with WAIT_ANY / WAIT_ALL.
//
// An event object holds a 32-bit flag word.  Any number of tasks can block in
// xRTOS_Event_Wait simultaneously, each with its own mask and mode (ANY/ALL).
// When Set raises flags, every blocked task whose condition is now satisfied is
// woken at once (multicast).  CLEAR_ON_EXIT removes the matched bits from the
// event's flag word when the task is woken or when Wait returns on fast path.
//
// Wait metadata lives in the blocked task's block_payload field, so event
// object RAM does not grow with xRTOS_MAX_TASKS.
//
// Usage:
//
//   xRTOS_Event_Context_t evt;
//   xRTOS_Event_Init(&evt, "MyEvt");
//
//   // Task: wait for bit 0 or bit 1, clear on exit.
//   uint32_t got;
//   xRTOS_Event_Wait(&evt, 0x03U,
//                    xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT,
//                    xRTOS_WAIT_FOREVER, &got);
//
//   // ISR: set bit 0.
//   bool yield;
//   xRTOS_Event_Set_From_ISR(&evt, 0x01U, &yield);
//   if (yield) { port_yield(); }
//

#ifndef XRTOS_EVENT_H
#define XRTOS_EVENT_H

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

    // Wait mode options - combine with bitwise OR.
#define xRTOS_EVENT_WAIT_ANY      (1U << 0U) // Wake if ANY watched bit is set.
#define xRTOS_EVENT_WAIT_ALL      (1U << 1U) // Wake only when ALL watched bits are set.
#define xRTOS_EVENT_CLEAR_ON_EXIT (1U << 2U) // Clear matched bits from flags on wake.

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xRTOS_Event_Context_t
    {
        uint32_t flags;          // Current set of event flags.
        xRTOS_Bitmap_t wait_map; // Task-id-indexed map of blocked waiters.
        const char *name;        // Optional display name (NULL = unnamed). Set after Init.

    } xRTOS_Event_Context_t;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize event object: clears all flags and wait_map.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if event_ctx is NULL.
    xRETURN_t xRTOS_Event_Init(xRTOS_Event_Context_t *event_ctx, const char *name);

    // Set one or more flags. Wakes all blocked tasks whose wait condition is now
    // satisfied. Tasks are woken in order of descending urgency (lowest priority
    // number first). Multicast: all satisfied waiters are woken, not just one.
    // Must be called from task context; use Set_From_ISR from an ISR.
    xRETURN_t xRTOS_Event_Set(xRTOS_Event_Context_t *event_ctx, uint32_t flags);

    // ISR-safe variant of xRTOS_Event_Set. should_yield is set to true when at
    // least one woken task has strictly higher priority than the current task.
    // should_yield must not be NULL; NULL returns xRETURN_xERR_xRTOS_NULL_POINTER.
    xRETURN_t xRTOS_Event_Set_From_ISR(xRTOS_Event_Context_t *event_ctx, uint32_t flags, bool *should_yield);

    // Clear one or more flags. Does not affect blocked tasks.
    // Returns xRETURN_xERR_xRTOS_NULL_POINTER if event_ctx is NULL.
    xRETURN_t xRTOS_Event_Clear(xRTOS_Event_Context_t *event_ctx, uint32_t flags);

    // Wait for event flags. flags is the mask of bits of interest.
    // options is a bitwise OR of xRTOS_EVENT_WAIT_* constants.
    // Exactly one of WAIT_ANY or WAIT_ALL must be set. flags must be non-zero.
    //
    // Fast path: if the condition is already satisfied, returns OK immediately.
    //   If CLEAR_ON_EXIT, matched bits are cleared before returning.
    //   *matched_flags is written with the bits that matched (may be NULL).
    //
    // If NO_WAIT and not satisfied: returns xRETURN_xERR_xRTOS_WOULD_BLOCK.
    //
    // Blocking path: task is blocked until Set satisfies its condition or the
    //   timeout fires. On wake: *matched_flags is written with the matched bits.
    //   Returns xRETURN_xRTOS_OK or xRETURN_xERR_xRTOS_TIMEOUT.
    //
    // Must be called from task context only. Returns INVALID_STATE if the port
    // reports ISR context.
    xRETURN_t
    xRTOS_Event_Wait(xRTOS_Event_Context_t *event_ctx, uint32_t flags, uint32_t options, uint32_t timeout_ticks, uint32_t *matched_flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_EVENT_H
// EOF /////////////////////////////////////////////////////////////////////////////
