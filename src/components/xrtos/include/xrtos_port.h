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

// @file xrtos_port.h
// @brief xRTOS CPU port operations interface.
//
// All CPU-specific context switch, interrupt masking, and tick code lives
// behind xRTOS_Port_Ops_t. The portable kernel calls through this table
// and never reads or writes CPU registers directly.
//
// Each supported CPU target provides a concrete xRTOS_Port_Ops_t instance:
//   - port/fake/xrtos_port_fake.c           - host (PC) stub for unit testing
//   - port/arm_r5/xrtos_port_arm_r5.c       - Cortex-R5 common core port
//   - port/qemu/arm_r5/xrtos_port_qemu_r5.c  - Cortex-R5 QEMU platform integrations
//   - port/hil/am243x/xrtos_port_am243x.c   - Cortex-R5 physical AM243x platform integrations
//
// Tick ISR integration contract
// -----------------------------
// Platform board startup code must:
//   1. Configure a hardware timer to fire at the desired RTOS tick rate.
//   2. In the timer ISR call:
//        bool should_yield = false;
//        xRTOS_Tick_Increment_From_ISR(&should_yield);
//        if (should_yield)
//        {
//            port_ops->yield();
//        }
//   3. The `yield` callback pends the context-switch interrupt for the port
//      (e.g. an ARM software-generated interrupt or SVC).
// No #ifdef shall appear in portable kernel code for tick wiring; the
// dependency flows entirely through port_ops->yield and the tick API.
//

#ifndef XRTOS_PORT_H
#define XRTOS_PORT_H

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
#include "xrtos_task.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xRTOS_Port_Ops_t
    {
        // Set up the initial stack frame for a new task.
        // On return, task_ctx->stack_top points to the CPU-ready frame.
        // The port shall set the stack's return address to xRTOS_Task_Exit so
        // that a task returning from its entry function terminates cleanly.
        void (*init_task_stack)(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg);

        // Restore the CPU context for task_ctx and begin execution.
        // This function must not return on real hardware.
        // Host stubs may return (treated as a no-op).
        void (*start_first_task)(xRTOS_Task_Context_t *task_ctx);

        // Request an immediate context switch.
        // On Cortex-R5 this pends a software-triggered interrupt (e.g. SVC/SGI).
        // On the host stub this sets is_schedule_pending = true.
        void (*yield)(void);

        // Disable CPU interrupts and return the previous interrupt-enable state.
        // The returned value is opaque to the kernel; pass it to enable_interrupts.
        uint32_t (*disable_interrupts)(void);

        // Restore interrupt state previously returned by disable_interrupts.
        void (*enable_interrupts)(uint32_t saved_state);

        // Returns true when the caller is executing inside an interrupt handler,
        // false otherwise. Used by synchronization primitives to select the
        // correct variant of blocking vs. non-blocking behaviour when called
        // from ISR context (e.g. give-from-ISR vs. give from task).
        bool (*is_in_isr)(void);

    } xRTOS_Port_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PORT_H
// EOF /////////////////////////////////////////////////////////////////////////////
