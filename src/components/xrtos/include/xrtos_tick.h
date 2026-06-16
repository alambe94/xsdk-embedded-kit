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

// @file xrtos_tick.h
// @brief xRTOS tick counter, task delay, and timeout processing APIs.
//
// Tick model:
//   - kernel->tick_count is a free-running uint32_t incremented by every call
//     to xRTOS_Tick_Increment_From_ISR. It wraps at UINT32_MAX.
//   - All timeout comparisons use the wrap-safe helper xRTOS_Tick_Has_Expired
//     to handle the 32-bit rollover correctly, provided that individual delay
//     values do not exceed 2^31 - 1 ticks.
//
// Timeout map:
//   - kernel->timeout_map is a task-id-indexed bitmap mirroring the scheduler
//     bitmaps. Bit T set means task_table[T] is in a timed wait and has a valid
//     wake_tick stored in task_ctx->wake_tick.
//   - xRTOS_Task_Delay arms the timeout_map; xRTOS_Tick_Increment_From_ISR
//     clears it when a task's wake_tick is reached.
//
// ISR integration (AM243x):
//   Configure one RTI (Real-Time Interrupt) channel as the RTOS tick source.
//   In the ISR body:
//
//     bool should_yield;
//     xRTOS_Tick_Increment_From_ISR(&should_yield);
//     if (should_yield)
//     {
//         kernel->port_ops->yield();   // or equivalent SGI write
//     }
//

#ifndef XRTOS_TICK_H
#define XRTOS_TICK_H

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
#include "xrtos_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Returns true when tick_count has reached or passed wake_tick.
    // Handles 32-bit wrap-around correctly for delays up to 2^31 - 1 ticks.
    static inline bool xRTOS_Tick_Has_Expired(uint32_t tick_count, uint32_t wake_tick)
    {
        return ((int32_t)(tick_count - wake_tick)) >= 0;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Returns the current kernel tick counter value.
    // Thread-safe on targets where 32-bit reads are atomic.
    uint32_t xRTOS_Tick_Get(void);

    // Delay the calling task for ticks kernel ticks.
    // The task transitions to BLOCKED and is placed in the timeout_map.
    // Returns to the caller when ticks elapse (driven by xRTOS_Tick_Increment_From_ISR).
    //
    // ticks == 0: yields without blocking (sets is_schedule_pending).
    // ticks == xRTOS_WAIT_FOREVER: undefined behaviour (assert in debug builds).
    //
    // Must be called from task context, not ISR context.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE if the kernel is not started or
    // if the port reports ISR context.
    xRETURN_t xRTOS_Task_Delay(uint32_t ticks);

    // Increment the kernel tick counter and process expired timeouts.
    // Must be called from the hardware timer ISR configured as the tick source.
    //
    // For each task in timeout_map whose wake_tick has been reached, the task is
    // unblocked with block_status xRETURN_xERR_xRTOS_TIMEOUT and its bit is
    // cleared from timeout_map.
    //
    // *should_yield is set to true if any pending context switch is outstanding
    // after processing (is_schedule_pending is true), false otherwise. The ISR
    // caller shall request a port yield when should_yield is true.
    //
    // should_yield must not be NULL. Passing NULL triggers xASSERT in debug
    // builds and returns without writing in release builds.
    void xRTOS_Tick_Increment_From_ISR(bool *should_yield);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_TICK_H
// EOF /////////////////////////////////////////////////////////////////////////////
