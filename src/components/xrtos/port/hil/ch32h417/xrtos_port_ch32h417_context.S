// Copyright 2022 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

    .section .text.xrtos_ch32h417_context, "ax", @progbits
    .align 2

    .equ XRTOS_CH32H417_CONTEXT_WORDS, 30
    .equ XRTOS_CH32H417_CONTEXT_BYTES, 120
    .equ XRTOS_CH32H417_SW_IRQ_BIT, 0x4000
    .equ XRTOS_CH32H417_PFIC_IPRR1, 0xE000E280
    .equ XRTOS_CH32H417_HW_POP_OFF, 0x20
    .equ XRTOS_CH32H417_USER_IRQ_ENABLE, 0x88

    .global SW_Handler
    .type SW_Handler, @function
SW_Handler:
    addi sp, sp, -XRTOS_CH32H417_CONTEXT_BYTES
    sw x1, 4(sp)
    sw x5, 8(sp)
    sw x6, 12(sp)
    sw x7, 16(sp)
    sw x8, 20(sp)
    sw x9, 24(sp)
    sw x10, 28(sp)
    sw x11, 32(sp)
    sw x12, 36(sp)
    sw x13, 40(sp)
    sw x14, 44(sp)
    sw x15, 48(sp)
    sw x16, 52(sp)
    sw x17, 56(sp)
    sw x18, 60(sp)
    sw x19, 64(sp)
    sw x20, 68(sp)
    sw x21, 72(sp)
    sw x22, 76(sp)
    sw x23, 80(sp)
    sw x24, 84(sp)
    sw x25, 88(sp)
    sw x26, 92(sp)
    sw x27, 96(sp)
    sw x28, 100(sp)
    sw x29, 104(sp)
    sw x30, 108(sp)
    sw x31, 112(sp)

    csrr t0, mepc
    sw t0, 0(sp)
    csrr t0, mstatus
    sw t0, 116(sp)
    mv s0, sp

    li t0, XRTOS_CH32H417_HW_POP_OFF
    csrs 0x804, t0

    li t0, XRTOS_CH32H417_PFIC_IPRR1
    li t1, XRTOS_CH32H417_SW_IRQ_BIT
    sw t1, 0(t0)

    call xRTOS_Scheduler_Current_Task
    beqz a0, xrtos_ch32h417_restore_current
    sw sp, 0(a0)

    la sp, xRTOS_CH32H417_ISR_Stack
    addi sp, sp, 512
    call xRTOS_Port_CH32H417_Switch_Context

    call xRTOS_Scheduler_Current_Task
    beqz a0, xrtos_ch32h417_restore_saved
    lw sp, 0(a0)
    j xrtos_ch32h417_restore_current

xrtos_ch32h417_restore_saved:
    mv sp, s0

xrtos_ch32h417_restore_current:
    lw t0, 0(sp)
    csrw mepc, t0
    lw t0, 116(sp)
    csrw mstatus, t0

    lw x1, 4(sp)
    lw x5, 8(sp)
    lw x6, 12(sp)
    lw x7, 16(sp)
    lw x8, 20(sp)
    lw x9, 24(sp)
    lw x10, 28(sp)
    lw x11, 32(sp)
    lw x12, 36(sp)
    lw x13, 40(sp)
    lw x14, 44(sp)
    lw x15, 48(sp)
    lw x16, 52(sp)
    lw x17, 56(sp)
    lw x18, 60(sp)
    lw x19, 64(sp)
    lw x20, 68(sp)
    lw x21, 72(sp)
    lw x22, 76(sp)
    lw x23, 80(sp)
    lw x24, 84(sp)
    lw x25, 88(sp)
    lw x26, 92(sp)
    lw x27, 96(sp)
    lw x28, 100(sp)
    lw x29, 104(sp)
    lw x30, 108(sp)
    lw x31, 112(sp)
    addi sp, sp, XRTOS_CH32H417_CONTEXT_BYTES
    mret
    .size SW_Handler, . - SW_Handler

    .global xRTOS_Port_CH32H417_Start_First_Task
    .type xRTOS_Port_CH32H417_Start_First_Task, @function
xRTOS_Port_CH32H417_Start_First_Task:
    lw sp, 0(a0)
    lw x1, 0(sp)
    lw x6, 12(sp)
    lw x7, 16(sp)
    lw x8, 20(sp)
    lw x9, 24(sp)
    lw x10, 28(sp)
    lw x11, 32(sp)
    lw x12, 36(sp)
    lw x13, 40(sp)
    lw x14, 44(sp)
    lw x15, 48(sp)
    lw x16, 52(sp)
    lw x17, 56(sp)
    lw x18, 60(sp)
    lw x19, 64(sp)
    lw x20, 68(sp)
    lw x21, 72(sp)
    lw x22, 76(sp)
    lw x23, 80(sp)
    lw x24, 84(sp)
    lw x25, 88(sp)
    lw x26, 92(sp)
    lw x27, 96(sp)
    lw x28, 100(sp)
    lw x29, 104(sp)
    lw x30, 108(sp)
    lw x31, 112(sp)

    li x5, XRTOS_CH32H417_USER_IRQ_ENABLE
    csrs 0x800, x5
    lw x5, 8(sp)

    addi sp, sp, XRTOS_CH32H417_CONTEXT_BYTES
    ret
    .size xRTOS_Port_CH32H417_Start_First_Task, . - xRTOS_Port_CH32H417_Start_First_Task

// EOF /////////////////////////////////////////////////////////////////////////
