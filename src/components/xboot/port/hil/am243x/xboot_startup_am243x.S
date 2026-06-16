// Copyright 2026 alambe94
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

// @file xboot_startup_am243x.S
// @brief AM243x R5F reset vector and early initialization.
//

        .text
        .global _vectors_xboot
        .type   _vectors_xboot,%function
        .section ".vectors","ax",%progbits
        .arm
        .align 4

_vectors_xboot:
        LDR     pc, [pc, #24]  // Reset
        LDR     pc, [pc, #24]  // Undefined Instruction
        LDR     pc, [pc, #24]  // Software Interrupt
        LDR     pc, [pc, #24]  // Prefetch Abort
        LDR     pc, [pc, #24]  // Data Abort
        LDR     pc, [pc, #24]  // Reserved
        LDR     pc, [pc, #24]  // IRQ
        LDR     pc, [pc, #24]  // FIQ

        .long   xBOOT_Reset_Handler
        .long   undefined_instruction_handler
        .long   software_interrupt_handler
        .long   prefetch_abort_handler
        .long   data_abort_handler
        .long   reserved_handler
        .long   irq_handler
        .long   fiq_handler

        .text
        .global xBOOT_Reset_Handler
        .type   xBOOT_Reset_Handler,%function
        .section ".text.boot","ax",%progbits
        .arm
        .align 2

xBOOT_Reset_Handler:
        // Disable IRQ and FIQ
        MRS     r0, cpsr
        ORR     r0, r0, #0xC0
        MSR     cpsr_cf, r0

#ifdef __ARM_FP
        // Enable VFPv3-D16: CP10/CP11 full access, then FPEXC.EN
        MRC     p15, 0, r0, c1, c0, 2
        MOV     r3, #0xF00000
        ORR     r0, r0, r3
        MCR     p15, 0, r0, c1, c0, 2
        ISB
        MOV     r0, #0x40000000
        FMXR    FPEXC, r0
#endif

        // Switch to FIQ mode and initialize FIQ stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x11
        MSR     cpsr_cf, r0
        LDR     sp, c_FIQ_STACK_END

        // Switch to IRQ mode and initialize IRQ stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x12
        MSR     cpsr_cf, r0
        LDR     sp, c_IRQ_STACK_END

        // Switch to SVC mode and initialize SVC stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x13
        MSR     cpsr_cf, r0
        LDR     sp, c_SVC_STACK_END

        // Switch to Abort mode and initialize Abort stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x17
        MSR     cpsr_cf, r0
        LDR     sp, c_ABORT_STACK_END

        // Switch to Undefined mode and initialize Undefined stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x1B
        MSR     cpsr_cf, r0
        LDR     sp, c_UNDEFINED_STACK_END

        // Switch to System mode and initialize System stack
        MRS     r0, cpsr
        BIC     r0, r0, #0x1F
        ORR     r0, r0, #0x1F
        MSR     cpsr_cf, r0
        LDR     sp, c_STACK_END

        // Zero BSS
        LDR     r0, =__BSS_START
        LDR     r1, =__BSS_END
        MOV     r2, #0
1:
        CMP     r0, r1
        BHS     2f
        STR     r2, [r0], #4
        B       1b
2:
        // MPU and cache init
        BL      __mpu_init

        // Call xBOOT entry point
        BL      xBOOT_Main

3:
        WFI
        B       3b

// Trap handlers for exception vectors
undefined_instruction_handler:
        B       undefined_instruction_handler
software_interrupt_handler:
        B       software_interrupt_handler
prefetch_abort_handler:
        B       prefetch_abort_handler
data_abort_handler:
        B       data_abort_handler
reserved_handler:
        B       reserved_handler
irq_handler:
        B       irq_handler
fiq_handler:
        B       fiq_handler

c_FIQ_STACK_END:       .long __FIQ_STACK_END
c_IRQ_STACK_END:       .long __IRQ_STACK_END
c_SVC_STACK_END:       .long __SVC_STACK_END
c_ABORT_STACK_END:     .long __ABORT_STACK_END
c_UNDEFINED_STACK_END: .long __UNDEFINED_STACK_END
c_STACK_END:           .long __STACK_END

        .global __mpu_init
        .global xBOOT_Main
        .global __FIQ_STACK_END
        .global __IRQ_STACK_END
        .global __SVC_STACK_END
        .global __ABORT_STACK_END
        .global __UNDEFINED_STACK_END
        .global __STACK_END
        .global __BSS_START
        .global __BSS_END

        .end
