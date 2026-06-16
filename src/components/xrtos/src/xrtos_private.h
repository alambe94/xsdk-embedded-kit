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

// @file xrtos_private.h
// @brief Internal declarations shared between xRTOS kernel source files.
//        Not part of the public API - must not be included from outside src/.

#ifndef XRTOS_PRIVATE_H
#define XRTOS_PRIVATE_H

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
#include "xrtos_core.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Finish a scheduler-locked update that may have woken a higher-priority task.
    // The pending flag is sampled before interrupts are restored; any task-context
    // yield is requested only after the critical section has ended.
    static inline void xrtos_scheduler_unlock_and_maybe_yield(xRTOS_Kernel_Context_t *kernel, uint32_t saved_state, bool *should_yield)
    {
        xASSERT(kernel != NULL, "kernel is NULL");
        xASSERT(kernel->port_ops != NULL, "port_ops is NULL");

        bool pending = kernel->scheduler.is_schedule_pending;

        xRTOS_Scheduler_Unlock(saved_state);

        if (should_yield != NULL)
        {
            *should_yield = pending;
        }
        else if (pending && !kernel->port_ops->is_in_isr())
        {
            kernel->port_ops->yield();
        }
        else
        {
            // No action required.
        }
    }

    static inline bool xrtos_port_reports_isr(const xRTOS_Kernel_Context_t *kernel)
    {
        return (kernel != NULL) && (kernel->port_ops != NULL) && (kernel->port_ops->is_in_isr != NULL) && kernel->port_ops->is_in_isr();
    }

    static inline bool xrtos_blocking_call_is_from_isr(const xRTOS_Kernel_Context_t *kernel)
    {
        bool is_in_isr = xrtos_port_reports_isr(kernel);
        xASSERT(!is_in_isr, "blocking API called from ISR");
        return is_in_isr;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // --- Global kernel instance ---

    // Returns the kernel pointer set by xRTOS_Kernel_Init, or NULL if not yet called.
    xRTOS_Kernel_Context_t *xrtos_kernel_get(void);

    // --- Scheduler internals (callers must hold the scheduler lock) ---

    // Add task_id to the ready list using an already-validated kernel pointer.
    void xrtos_scheduler_ready_add(xRTOS_Kernel_Context_t *kernel, uint32_t task_id);

    // Remove task_id from the ready list. No-ops silently when task is not ready.
    void xrtos_scheduler_ready_remove(xRTOS_Kernel_Context_t *kernel, uint32_t task_id);

    // --- Mutex PI hooks ---

    // Relink task_ctx in the mutex sorted waiter list after its priority changed.
    // Does NOT propagate the change to the mutex owner; the caller is responsible
    // for PI propagation (mutex_recompute_task_priority tail-call or xrtos_mutex_owner_recompute).
    void xrtos_mutex_waiter_priority_changed(xRTOS_Task_Context_t *task_ctx, uint32_t old_priority);

    // Propagate a priority change from a blocked task to the mutex owner chain.
    // Call this from top-level priority-change paths (Set_Priority) AFTER calling
    // xrtos_mutex_waiter_priority_changed.  Do NOT call from within the PI recursion
    // (mutex_recompute_task_priority already handles propagation via its tail call).
    void xrtos_mutex_owner_recompute(xRTOS_Task_Context_t *task_ctx, uint32_t old_priority);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PRIVATE_H
// EOF /////////////////////////////////////////////////////////////////////////////
