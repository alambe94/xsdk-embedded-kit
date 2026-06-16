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

// @file xrtos_notify.h
// @brief xRTOS task notification APIs.
//
// Task notifications are the preferred lightweight signal mechanism. Each task
// has a single 32-bit notification value and a pending flag stored directly in
// its xRTOS_Task_Context_t. No separate object allocation is required.
//
// Notify APIs OR the supplied value into task->notify_value and set
// has_notify_pending. If the target task is blocked in xRTOS_Task_Notify_Wait,
// it is woken immediately; otherwise the notification is latched for the next
// Notify_Wait call.
//
// xRTOS_Task_Notify_Wait usage pattern:
//
//   uint32_t bits;
//   xRETURN_t ret = xRTOS_Task_Notify_Wait(
//       0U,            // clear_on_entry: bits to clear before checking pending
//       0xFFFFFFFFU,   // clear_on_exit:  bits to clear after reading value
//       &bits,         // value output pointer (may be NULL)
//       xRTOS_WAIT_FOREVER);
//   if (ret == xRETURN_xRTOS_OK) { /* process bits */ }
//   if (ret == xRETURN_xERR_xRTOS_TIMEOUT) { /* timed out */ }
//
// Notification value accumulation:
//   The value is OR-ed on each Notify call. clear_on_exit controls which bits
//   are cleared when the waiter collects the value (default: clear all with
//   0xFFFFFFFFU). This allows multiple event sources to set independent bits
//   that are collected and cleared atomically by a single Notify_Wait.
//
// ISR safety:
//   xRTOS_Task_Notify_From_ISR is safe to call from the timer or peripheral ISR.
//   xRTOS_Task_Notify may be called from task context only.
//   xRTOS_Task_Notify_Wait must be called from task context only.
//

#ifndef XRTOS_NOTIFY_H
#define XRTOS_NOTIFY_H

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

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Send a notification to the task identified by task_id. value is OR-ed into
    // task->notify_value and has_notify_pending is set. If the task is currently
    // blocked in xRTOS_Task_Notify_Wait, it latches the pre-clear value for
    // the waiter, applies clear_on_exit, and wakes it with block_status OK.
    // Must be called from task context.
    // Returns xRETURN_xERR_xRTOS_INVALID_ARGUMENT for an unknown task_id.
    xRETURN_t xRTOS_Task_Notify(uint32_t task_id, uint32_t value);

    // ISR-safe variant of xRTOS_Task_Notify. Should_yield is set to true when
    // the woken task has a strictly higher priority than the currently running
    // task; the ISR caller shall request a port yield when should_yield is true.
    // should_yield must not be NULL; NULL returns xRETURN_xERR_xRTOS_NULL_POINTER.
    xRETURN_t xRTOS_Task_Notify_From_ISR(uint32_t task_id, uint32_t value, bool *should_yield);

    // Wait for a notification on the calling task.
    //
    // clear_on_entry  - bits to clear in task->notify_value before checking pending.
    // clear_on_exit   - bits to clear in task->notify_value after collecting value.
    // value           - output: receives notify_value on OK return (may be NULL).
    // timeout_ticks   - xRTOS_NO_WAIT (0): return immediately with WOULD_BLOCK if
    //                   not pending. xRTOS_WAIT_FOREVER: block indefinitely.
    //                   Any other value: block until a notification arrives or
    //                   the timeout elapses.
    //
    // Returns xRETURN_xRTOS_OK on success (notification received).
    // Returns xRETURN_xERR_xRTOS_WOULD_BLOCK when no pending notification and
    //   timeout_ticks == xRTOS_NO_WAIT.
    // Returns xRETURN_xERR_xRTOS_TIMEOUT when the wait timed out.
    // Returns xRETURN_xERR_xRTOS_INVALID_STATE when the kernel is not started or
    // when the port reports ISR context.
    xRETURN_t xRTOS_Task_Notify_Wait(uint32_t clear_on_entry, uint32_t clear_on_exit, uint32_t *value, uint32_t timeout_ticks);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_NOTIFY_H
// EOF /////////////////////////////////////////////////////////////////////////////
