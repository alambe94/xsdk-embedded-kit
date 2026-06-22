#if !defined(BOARD_ZYNQ_A9) && !defined(BOARD_REALVIEW_PB_A8)
#define BOARD_ZYNQ_A9
#endif

    .syntax unified
    .arm

    .section .text.startup, "ax"
    .global _start
_start:
    // Exception Vector Table
    B       reset_handler        // Reset
    B       undef_handler        // Undefined Instruction
    B       svc_handler          // Software Interrupt (SVC)
    B       prefetch_handler     // Prefetch Abort
    B       data_handler         // Data Abort
    NOP                          // Reserved
    B       xRTOS_Port_ARM_R5_IRQ_Handler // IRQ
    B       fiq_handler          // FIQ

undef_handler:
#ifdef BOARD_ZYNQ_A9
    LDR     r1, =0xFF000030
    LDR     r2, =0xE0000030
    MOV     r3, #85  // 'U'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #78  // 'N'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
    STR     r3, [r2]
#elif defined(BOARD_REALVIEW_PB_A8)
    LDR     r1, =0x10009000
    MOV     r3, #85  // 'U'
    STR     r3, [r1]
    MOV     r3, #78  // 'N'
    STR     r3, [r1]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
#endif
1:  B       1b

prefetch_handler:
#ifdef BOARD_ZYNQ_A9
    LDR     r1, =0xFF000030
    LDR     r2, =0xE0000030
    MOV     r3, #80  // 'P'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #65  // 'A'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
    STR     r3, [r2]
#elif defined(BOARD_REALVIEW_PB_A8)
    LDR     r1, =0x10009000
    MOV     r3, #80  // 'P'
    STR     r3, [r1]
    MOV     r3, #65  // 'A'
    STR     r3, [r1]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
#endif
1:  B       1b

data_handler:
#ifdef BOARD_ZYNQ_A9
    LDR     r1, =0xFF000030
    LDR     r2, =0xE0000030
    MOV     r3, #68  // 'D'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #65  // 'A'
    STR     r3, [r1]
    STR     r3, [r2]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
    STR     r3, [r2]
#elif defined(BOARD_REALVIEW_PB_A8)
    LDR     r1, =0x10009000
    MOV     r3, #68  // 'D'
    STR     r3, [r1]
    MOV     r3, #65  // 'A'
    STR     r3, [r1]
    MOV     r3, #10  // '\n'
    STR     r3, [r1]
#endif
1:  B       1b

fiq_handler:        B fiq_handler

svc_handler:
    B       xRTOS_Port_ARM_R5_Context_Switch

reset_handler:
    // Disable interrupts initially (both IRQ and FIQ)
    CPSID   if

    // Set Vector Base Address Register (VBAR) to our vector table base (_start)
    LDR     r0, =_start
    MCR     p15, 0, r0, c12, c0, 0

    // Enable VFP: grant full access to CP10/CP11 via CPACR.
    // QEMU starts with FPEXC.EN=1 so no explicit VMSR needed.
    MRC     p15, 0, r0, c1, c0, 2
    ORR     r0, r0, #(0xF << 20)
    MCR     p15, 0, r0, c1, c0, 2
    ISB

    // Set up stack pointer for SVC mode (0x13)
    CPS     #0x13
    LDR     sp, =_svc_stack_top

    // Set up stack pointer for SYS mode (0x1F)
    CPS     #0x1F
    LDR     sp, =_sys_stack_top

    // Clear BSS
    LDR     r0, =_bss_start
    LDR     r1, =_bss_end
    MOV     r2, #0
bss_loop:
    CMP     r0, r1
    BGE     bss_done
    STR     r2, [r0], #4
    B       bss_loop
bss_done:

    // Branch to C main
    BL      main

    // Loop forever if main returns
1:  B       1b
