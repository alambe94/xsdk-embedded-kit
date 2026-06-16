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
//
// @file xrtos_port_arm_r5_asm.S
// @brief xRTOS ARM Cortex-R5 port - assembly context switch.
//

    .syntax unified
    .arm

    .section .text.xrtos_port, "ax"

// -----------------------------------------------------------------------------
// xRTOS_Port_ARM_R5_Context_Switch
// Callable from exception mode only (SVC / SGI ISR).
// -----------------------------------------------------------------------------
    .global xRTOS_Port_ARM_R5_Context_Switch
    .type   xRTOS_Port_ARM_R5_Context_Switch, %function
xRTOS_Port_ARM_R5_Context_Switch:
    // We enter in SVC mode (0x13).
    // The task was running in SYS mode (0x1F) before the SVC exception.
    // lr_svc holds the return address (PC of task).
    // spsr_svc holds the task's CPSR.

    // 1. Switch to SYS mode to use the task's stack.
    CPS     #0x1F

    // Allocate space for the 16-register frame (64 bytes).
    SUB     sp, sp, #64

    // Save task's original r0-r3, r12, and lr onto the SYS stack.
    STR     r0, [sp, #36]
    STR     r1, [sp, #40]
    STR     r2, [sp, #44]
    STR     r3, [sp, #48]
    STR     r12, [sp, #52]
    STR     lr, [sp, #56]

    // Switch back to SVC mode to retrieve spsr_svc and lr_svc.
    CPS     #0x13
    MRS     r0, spsr
    MOV     r1, lr

    // Switch back to SYS mode.
    CPS     #0x1F
    STR     r0, [sp, #32]        // Save task CPSR
    STR     r1, [sp, #60]        // Save task PC

    // Save callee-saved r4-r11 registers.
    STM     sp, {r4-r11}

    // Call C helper to get current task and save its sp.
    BL      xRTOS_Scheduler_Current_Task
    STR     sp, [r0]             // task_ctx->stack_top = sp

    // Transition yielding task to READY.
    BL      xRTOS_Port_ARM_R5_Pre_Switch

    // Select the next task to run.
    BL      xRTOS_Scheduler_Select_Next

    // Switch scheduler to next task.
    BL      xRTOS_Scheduler_Switch

    // Get the new task's context and load its sp.
    BL      xRTOS_Scheduler_Current_Task
    LDR     sp, [r0]             // sp = task_ctx->stack_top

    // Load new task's cpsr and pc from the frame.
    LDR     r0, [sp, #32]        // cpsr
    LDR     r1, [sp, #60]        // pc

    // Switch to SVC mode to set up spsr_svc and lr_svc.
    CPS     #0x13
    MSR     spsr_cxsf, r0
    MOV     lr, r1

    // Switch to SYS mode to restore the task's SYS mode registers.
    CPS     #0x1F
    LDR     r0, [sp, #36]
    LDR     r1, [sp, #40]
    LDR     r2, [sp, #44]
    LDR     r3, [sp, #48]
    LDR     r12, [sp, #52]
    LDR     lr, [sp, #56]

    LDM     sp, {r4-r11}
    ADD     sp, sp, #64          // Discard frame from SYS stack

    // Switch back to SVC mode for exception return.
    CPS     #0x13
    MOVS    pc, lr
    .size   xRTOS_Port_ARM_R5_Context_Switch, . - xRTOS_Port_ARM_R5_Context_Switch

// -----------------------------------------------------------------------------
// xRTOS_Port_ARM_R5_Start_First_Task
// Called by C-code (arm_r5_start_first_task) in SYS mode to boot the scheduler.
// -----------------------------------------------------------------------------
    .global xRTOS_Port_ARM_R5_Start_First_Task
    .type   xRTOS_Port_ARM_R5_Start_First_Task, %function
xRTOS_Port_ARM_R5_Start_First_Task:
    // r0 contains task_ctx pointer.
    LDR     sp, [r0]             // Load sp from task_ctx->stack_top

    // Load the task's cpsr and pc.
    LDR     r1, [sp, #32]        // cpsr
    LDR     r2, [sp, #60]        // pc

    // Switch to SVC mode to set up return state in spsr_svc and lr_svc.
    CPS     #0x13
    MSR     spsr_cxsf, r1
    MOV     lr, r2

    // Switch to SYS mode to load task's SYS mode registers.
    CPS     #0x1F
    LDR     r0, [sp, #36]
    LDR     r1, [sp, #40]
    LDR     r2, [sp, #44]
    LDR     r3, [sp, #48]
    LDR     r12, [sp, #52]
    LDR     lr, [sp, #56]

    LDM     sp, {r4-r11}
    ADD     sp, sp, #64          // Discard frame from SYS stack

    // Switch back to SVC mode and boot first task via exception return.
    CPS     #0x13
    MOVS    pc, lr
    .size   xRTOS_Port_ARM_R5_Start_First_Task, . - xRTOS_Port_ARM_R5_Start_First_Task

// -----------------------------------------------------------------------------
// xRTOS_Port_ARM_R5_IRQ_Handler
// Exception handler for IRQ interrupts.
// -----------------------------------------------------------------------------
    .global xRTOS_Port_ARM_R5_IRQ_Handler
    .type   xRTOS_Port_ARM_R5_IRQ_Handler, %function
xRTOS_Port_ARM_R5_IRQ_Handler:
    // We enter in IRQ mode (0x12).
    // lr_irq holds the return address + 4.
    // spsr_irq holds the interrupted task's CPSR.

    // 1. Adjust return address (lr points to next instruction, we subtract 4 to repeat/resume)
    SUB     lr, lr, #4

    // 2. Switch to SYS mode to use the task's stack
    CPS     #0x1F

    // Allocate space for the 16-register frame (64 bytes).
    SUB     sp, sp, #64

    // Save task's original r0-r3, r12, and lr onto the SYS stack.
    STR     r0, [sp, #36]
    STR     r1, [sp, #40]
    STR     r2, [sp, #44]
    STR     r3, [sp, #48]
    STR     r12, [sp, #52]
    STR     lr, [sp, #56]

    // Switch back to IRQ mode to retrieve spsr_irq and lr_irq
    CPS     #0x12
    MRS     r0, spsr
    MOV     r1, lr

    // Switch back to SYS mode.
    CPS     #0x1F
    STR     r0, [sp, #32]        // Save task CPSR
    STR     r1, [sp, #60]        // Save task PC

    // Save callee-saved r4-r11 registers.
    STM     sp, {r4-r11}

    // Call C helper to save the stack pointer of the interrupted task
    BL      xRTOS_Scheduler_Current_Task
    STR     sp, [r0]             // task_ctx->stack_top = sp

    // Call the C IRQ handler to process timer interrupts/ticks
    BL      xrtos_port_arm_r5_irq_handler

    // Check if we should switch context (reschedule pending)
    BL      xrtos_port_arm_r5_should_switch
    CMP     r0, #0
    BEQ     irq_restore_context

    // Preemption: run the scheduler to switch to the new task
    BL      xRTOS_Port_ARM_R5_Pre_Switch
    BL      xRTOS_Scheduler_Select_Next
    BL      xRTOS_Scheduler_Switch

irq_restore_context:
    // Get the new/current task's context and load its sp.
    BL      xRTOS_Scheduler_Current_Task
    LDR     sp, [r0]             // sp = task_ctx->stack_top

    // Load task's cpsr and pc from the frame.
    LDR     r0, [sp, #32]        // cpsr
    LDR     r1, [sp, #60]        // pc

    // Switch to IRQ mode to set up spsr_irq and lr_irq.
    CPS     #0x12
    MSR     spsr_cxsf, r0
    MOV     lr, r1

    // Switch to SYS mode to restore the task's SYS mode registers.
    CPS     #0x1F
    LDR     r0, [sp, #36]
    LDR     r1, [sp, #40]
    LDR     r2, [sp, #44]
    LDR     r3, [sp, #48]
    LDR     r12, [sp, #52]
    LDR     lr, [sp, #56]

    LDM     sp, {r4-r11}
    ADD     sp, sp, #64          // Discard frame from SYS stack

    // Switch back to IRQ mode for exception return.
    CPS     #0x12
    MOVS    pc, lr
    .size   xRTOS_Port_ARM_R5_IRQ_Handler, . - xRTOS_Port_ARM_R5_IRQ_Handler
