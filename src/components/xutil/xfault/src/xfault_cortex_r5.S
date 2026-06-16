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
// @file xfault_cortex_r5.S
// @brief xFAULT Cortex-R5 exception trampolines.
//
// These handlers are intended to be branched to by the board/startup exception
// vector table while running in ARM state. They save the interrupted register
// state into xFAULT_Exception_Frame_t, then call the C fatal path. The current
// exception-mode stack is used for the temporary frame.

    .syntax unified
    .arm

    .section .text.xfault_cortex_r5, "ax"

    .equ XFAULT_FRAME_R0,             0
    .equ XFAULT_FRAME_SP,             52
    .equ XFAULT_FRAME_LR,             56
    .equ XFAULT_FRAME_PC,             60
    .equ XFAULT_FRAME_CPSR,           64
    .equ XFAULT_FRAME_SPSR,           68
    .equ XFAULT_FRAME_EXCEPTION_TYPE, 72
    .equ XFAULT_FRAME_SIZE,           76

    .equ XFAULT_TYPE_DATA_ABORT,      1
    .equ XFAULT_TYPE_PREFETCH_ABORT,  2
    .equ XFAULT_TYPE_UNDEFINED,       3

    .macro XFAULT_SAVE_AND_CALL exception_type, pc_adjust
    sub     sp, sp, #XFAULT_FRAME_SIZE
    stmia   sp, {r0-r12}

    mrs     r1, spsr
    mrs     r2, cpsr
    str     r1, [sp, #XFAULT_FRAME_SPSR]
    str     r2, [sp, #XFAULT_FRAME_CPSR]

    mov     r3, lr
    sub     r3, r3, #\pc_adjust
    str     r3, [sp, #XFAULT_FRAME_PC]

    mov     r12, sp
    bic     r3, r2, #0x1F
    orr     r3, r3, #0x1F
    msr     cpsr_c, r3
    str     sp, [r12, #XFAULT_FRAME_SP]
    str     lr, [r12, #XFAULT_FRAME_LR]
    msr     cpsr_c, r2

    mov     r3, #\exception_type
    str     r3, [sp, #XFAULT_FRAME_EXCEPTION_TYPE]

    mov     r0, sp
    bl      xFAULT_Fatal_From_Exception_Frame

1:
    b       1b
    .endm

    .global xFAULT_Data_Abort_Handler
    .type   xFAULT_Data_Abort_Handler, %function
xFAULT_Data_Abort_Handler:
    XFAULT_SAVE_AND_CALL XFAULT_TYPE_DATA_ABORT, 8
    .size   xFAULT_Data_Abort_Handler, . - xFAULT_Data_Abort_Handler

    .global xFAULT_Prefetch_Abort_Handler
    .type   xFAULT_Prefetch_Abort_Handler, %function
xFAULT_Prefetch_Abort_Handler:
    XFAULT_SAVE_AND_CALL XFAULT_TYPE_PREFETCH_ABORT, 4
    .size   xFAULT_Prefetch_Abort_Handler, . - xFAULT_Prefetch_Abort_Handler

    .global xFAULT_Undefined_Instruction_Handler
    .type   xFAULT_Undefined_Instruction_Handler, %function
xFAULT_Undefined_Instruction_Handler:
    XFAULT_SAVE_AND_CALL XFAULT_TYPE_UNDEFINED, 4
    .size   xFAULT_Undefined_Instruction_Handler, . - xFAULT_Undefined_Instruction_Handler

// EOF /////////////////////////////////////////////////////////////////////////
