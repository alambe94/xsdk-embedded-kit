/*
 * AM243x Cortex-R5 reset handler and startup sequence.
 *
 * Responsibilities:
 *   - disable IRQ/FIQ
 *   - enable FPU (hard-float build)
 *   - initialize mode stacks (FIQ, IRQ, SVC, ABORT, UNDEFINED, SYS)
 *   - zero BSS
 *   - call xRTOS_Port_AM243x_Early_Init (MPU + cache init)
 *   - set VBAR to _vectors (after MPU is up)
 *   - call main
 *   - WFI loop if main returns
 *
 * Modeled after TI MCU+ SDK boot_armv7r_asm.S to match tiarmclang behavior.
 * The vector table that references this handler lives in
 * src/components/xrtos/port/am243x/xrtos_port_am243x_vectors.S.
 */

        .text
        .global xRTOS_Port_AM243x_Reset_Handler
        .type   xRTOS_Port_AM243x_Reset_Handler,%function
        .section ".text.boot","ax",%progbits
        .arm
        .align 2

xRTOS_Port_AM243x_Reset_Handler:

        /* Disable IRQ and FIQ. */
        MRS     r0, cpsr
        ORR     r0, r0, #0xC0
        MSR     cpsr_cf, r0

#ifdef __ARM_FP
        /* Enable VFPv3-D16: grant CP10/CP11 full access, then set FPEXC.EN.
         * ISB is required after CPACR write before any VFP instruction. */
        MRC     p15, 0, r0, c1, c0, 2
        MOV     r3, #0xF00000
        ORR     r0, r0, r3
        MCR     p15, 0, r0, c1, c0, 2
        ISB
        MOV     r0, #0x40000000
        FMXR    FPEXC, r0
#endif

        /* Switch to FIQ mode and initialize FIQ stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x11
        MSR     cpsr_cf, r0
        LDR     sp, c_FIQ_STACK_END

        /* Switch to IRQ mode and initialize IRQ stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x12
        MSR     cpsr_cf, r0
        LDR     sp, c_IRQ_STACK_END

        /* Switch to SVC mode and initialize SVC stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x13
        MSR     cpsr_cf, r0
        LDR     sp, c_SVC_STACK_END

        /* Switch to Abort mode and initialize Abort stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x17
        MSR     cpsr_cf, r0
        LDR     sp, c_ABORT_STACK_END

        /* Switch to Undefined mode and initialize Undefined stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x1B
        MSR     cpsr_cf, r0
        LDR     sp, c_UNDEFINED_STACK_END

        /* Switch to System mode and initialize System stack. */
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x1F
        MSR     cpsr_cf, r0
        LDR     sp, c_STACK_END

        /* Zero BSS. */
        LDR     r0, =__BSS_START
        LDR     r1, =__BSS_END
        MOV     r2, #0
1:
        CMP     r0, r1
        BHS     2f
        STR     r2, [r0], #4
        B       1b
2:
        /* MPU and cache init (sets up memory regions, enables I/D cache). */
        BL      xRTOS_Port_AM243x_Early_Init

        /* Point VBAR at the xRTOS vector table after MPU is live.
         * Cortex-R5 DOES implement the CP15 c12 VBAR register (ARM DDI 0460D §4.3.4).
         * The write is omitted here because the linker places _vectors at 0x00000000
         * (TCMA) which matches the hardware reset default for VBAR, so the write is
         * not strictly necessary for correct operation.  Uncomment if the vector
         * table is ever relocated from 0x0. */
        // LDR     r0, =_vectors
        // MCR     p15, 0, r0, c12, c0, 0
        // ISB

        BL      main
3:
        WFI
        B       3b

c_FIQ_STACK_END:       .long __FIQ_STACK_END
c_IRQ_STACK_END:       .long __IRQ_STACK_END
c_SVC_STACK_END:       .long __SVC_STACK_END
c_ABORT_STACK_END:     .long __ABORT_STACK_END
c_UNDEFINED_STACK_END: .long __UNDEFINED_STACK_END
c_STACK_END:           .long __STACK_END

        .global _vectors
        .global xRTOS_Port_AM243x_Early_Init
        .global main
        .global __FIQ_STACK_END
        .global __IRQ_STACK_END
        .global __SVC_STACK_END
        .global __ABORT_STACK_END
        .global __UNDEFINED_STACK_END
        .global __STACK_END
        .global __BSS_START
        .global __BSS_END

        .end
