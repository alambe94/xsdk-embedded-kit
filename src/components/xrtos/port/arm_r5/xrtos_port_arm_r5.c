// Copyright 2022 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xrtos_port_arm_r5.c
// @brief xRTOS ARM Cortex-R5 port - C-language port callback stubs.
//
// Provides the xrtos_arm_r5_port_ops table and the C-implementable callbacks.
// The context-switch proper (xRTOS_Port_ARM_R5_Context_Switch) lives in
// xrtos_port_arm_r5_asm.S and is declared in xrtos_port_arm_r5.h.
//

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xrtos_core.h"
#include "xrtos_port.h"
#include "xrtos_port_arm_r5.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_private.h"

// CPSR initial value for a new task running in SYS mode with IRQ enabled.
// ARM Architecture Reference Manual, ARMv7-A/R, A2.5:
//   Bits [4:0] = 0x1F  -> SYS mode
//   Bit  [7]   = 0     -> IRQ unmasked (I = 0)
//   Bit  [6]   = 0     -> FIQ unmasked (F = 0); masked when XRTOS_CONFIG_DISABLE_FIQ
//   Bit  [5]   = 0     -> ARM state (T = 0)
#ifndef XRTOS_CONFIG_DISABLE_FIQ
#define ARM_R5_INIT_CPSR 0x0000001FU
#else
#define ARM_R5_INIT_CPSR 0x0000005FU // F-bit set
#endif

// PRIVATE FUNCTION PROTOTYPES /////////////////////////////////////////////////

extern void xRTOS_Port_ARM_R5_Start_First_Task(xRTOS_Task_Context_t *task_ctx);

static void arm_r5_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *entry_arg);

static void arm_r5_start_first_task(xRTOS_Task_Context_t *task_ctx);

static void arm_r5_yield(void);

static uint32_t arm_r5_disable_interrupts(void);

static void arm_r5_enable_interrupts(uint32_t saved_state);

static bool arm_r5_is_in_isr(void);

// PORT OPS TABLE //////////////////////////////////////////////////////////////

const xRTOS_Port_Ops_t xrtos_arm_r5_port_ops = {
    .init_task_stack = arm_r5_init_task_stack,
    .start_first_task = arm_r5_start_first_task,
    .yield = arm_r5_yield,
    .disable_interrupts = arm_r5_disable_interrupts,
    .enable_interrupts = arm_r5_enable_interrupts,
    .is_in_isr = arm_r5_is_in_isr,
};

// PRIVATE FUNCTIONS ///////////////////////////////////////////////////////////

// Builds the initial exception frame (xRTOS_Port_ARM_R5_Frame_t) at the top
// of the task's stack and updates task_ctx->stack_top to point to it.
//
// Stack layout (grows downward; frame base = lowest address in frame):
//   stack_base[stack_depth_words - 1]  <- highest address (stack ceiling)
//   ...
//   frame                              <- sizeof(xRTOS_Port_ARM_R5_Frame_t) bytes
//   task_ctx->stack_top -------------> &frame  (frame base)
//
static void arm_r5_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *entry_arg)
{
    if (task_ctx == NULL || entry == NULL)
    {
        return;
    }

    uint32_t *stack_base = task_ctx->stack_mem;
    uint32_t stack_depth_words = task_ctx->stack_words;

    if (stack_base == NULL || stack_depth_words < XRTOS_PORT_ARM_R5_MIN_STACK_WORDS)
    {
        return;
    }

    // Compute the top-of-stack pointer (one past the last valid word).
    uint32_t *stack_top = stack_base + stack_depth_words;

    // Align down to AAPCS 8-byte boundary.
    // Cast via uintptr_t to avoid pointer-arithmetic UB on different-sized types.
    uintptr_t top_addr = (uintptr_t)stack_top;
    top_addr &= ~((uintptr_t)(XRTOS_PORT_ARM_R5_STACK_ALIGN_BYTES - 1U));
    stack_top = (uint32_t *)top_addr;

    // Carve out the initial frame below the aligned top.
    xRTOS_Port_ARM_R5_Frame_t *frame = ((xRTOS_Port_ARM_R5_Frame_t *)stack_top) - 1;

    // Zero the frame so unused registers start at a deterministic value.
    (void)memset(frame, 0, sizeof(*frame));

    // Callee-saved GPRs - zero is fine for a freshly started task.
    frame->r4 = 0U;
    frame->r5 = 0U;
    frame->r6 = 0U;
    frame->r7 = 0U;
    frame->r8 = 0U;
    frame->r9 = 0U;
    frame->r10 = 0U;
    frame->r11 = 0U;

    // CPSR: SYS mode, IRQ enabled, ARM state.
    frame->cpsr = ARM_R5_INIT_CPSR;

    // Argument registers: r0 carries entry_arg; r1-r3 are scratch, set to 0.
    frame->r0 = (uint32_t)(uintptr_t)entry_arg;
    frame->r1 = 0U;
    frame->r2 = 0U;
    frame->r3 = 0U;
    frame->r12 = 0U;

    // lr points to the task-exit handler so a bare return from entry cleans up.
    frame->lr = (uint32_t)(uintptr_t)xRTOS_Task_Exit;

    // pc is the task entry function; the context switch restores this into PC.
    frame->pc = (uint32_t)(uintptr_t)entry;

    // stack_top stored in the task context is the base of the frame (lowest addr).
    task_ctx->stack_top = (uint32_t *)frame;
}

static void arm_r5_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    xRTOS_Port_ARM_R5_Start_First_Task(task_ctx);
}

static void arm_r5_yield(void)
{
    __asm volatile("svc #0" : : : "memory");
}

static uint32_t arm_r5_disable_interrupts(void)
{
    uint32_t saved_state;
    __asm volatile("mrs %0, cpsr\n"
                   "cpsid i\n"
                   : "=r"(saved_state)
                   :
                   : "memory");
    return saved_state;
}

static void arm_r5_enable_interrupts(uint32_t saved_state)
{
    __asm volatile("msr cpsr_c, %0\n" : : "r"(saved_state) : "memory");
}

static bool arm_r5_is_in_isr(void)
{
    uint32_t cpsr;
    __asm volatile("mrs %0, cpsr\n" : "=r"(cpsr));
    uint32_t mode = cpsr & 0x1FU;
    return (mode != 0x1FU) && (mode != 0x10U);
}

void xRTOS_Port_ARM_R5_Pre_Switch(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if (kernel != NULL && kernel->scheduler.is_started)
    {
        uint32_t current_id = kernel->scheduler.current_task_id;
        if (current_id < xRTOS_MAX_TASKS)
        {
            xRTOS_Task_Context_t *curr_task = kernel->task_table[current_id];
            if (curr_task != NULL && curr_task->state == xRTOS_TASK_STATE_RUNNING)
            {
                curr_task->state = xRTOS_TASK_STATE_READY;
                xrtos_scheduler_ready_add(kernel, current_id);
                xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_TASK_READY, current_id);
            }
        }
    }
}

__attribute__((weak)) void xrtos_port_arm_r5_irq_handler(void)
{
    // Default empty handler to satisfy link-time references for executables
    // that don't need interrupts or override it.
}

bool xrtos_port_arm_r5_should_switch(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    return (kernel != NULL) && kernel->scheduler.is_started && kernel->scheduler.is_schedule_pending;
}

// EOF /////////////////////////////////////////////////////////////////////////////

