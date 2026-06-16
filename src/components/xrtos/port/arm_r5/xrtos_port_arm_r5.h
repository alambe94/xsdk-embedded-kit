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

// @file xrtos_port_arm_r5.h
// @brief xRTOS ARM Cortex-R5 port - public declaration.
//
// This header is included by board startup code to obtain the
// xrtos_arm_r5_port_ops table passed to xRTOS_Kernel_Init.
//
// Stack requirements (AAPCS, ARM DDI 0406C):
//   - Minimum alignment : 8 bytes (AAPCS ABI requirement)
//   - Minimum depth     : XRTOS_PORT_ARM_R5_MIN_STACK_WORDS (32-bit words)
//
// Context switch model:
//   - Cortex-R5 in SYS mode (privileged; no User/SYS mode split needed for RTOS).
//   - No automatic hardware register stacking (unlike Cortex-M PendSV).
//   - All GPRs, CPSR, and optionally FPU registers are saved/restored by the
//     assembly context switch routine (xrtos_port_arm_r5_asm.S).
//   - FIQ is left unmasked unless xRTOS_CONFIG_DISABLE_FIQ is defined.
//
// Tick integration:
//   - Use a platform-specific timer (RTI for AM243x, Private Timer for QEMU) as the tick source.
//   - In the timer ISR call xRTOS_Tick_Increment_From_ISR then yield via
//     port_ops->yield.
//
// This file is NOT included by host tests. Host tests use xrtos_port_fake.h.
//

#ifndef XRTOS_PORT_ARM_R5_H
#define XRTOS_PORT_ARM_R5_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xrtos_port.h"

    // MACROS //////////////////////////////////////////////////////////////////////

// Minimum stack depth in 32-bit words. Includes the initial exception frame plus
// space for at least one nested function call from the task entry point.
// Increase for tasks that call deeply nested functions or use large local arrays.
#define XRTOS_PORT_ARM_R5_MIN_STACK_WORDS 64U

// Stack alignment required by AAPCS (in bytes). Stack pointer must be aligned
// to this value at every public function call boundary.
#define XRTOS_PORT_ARM_R5_STACK_ALIGN_BYTES 8U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Initial context frame layout built by arm_r5_init_task_stack.
    // The context switch assembly restores registers in this exact order.
    // The frame grows downward from task_ctx->stack_top - 1.
    //
    // Offset from frame base (lowest address in frame):
    //   [0]  r4      - callee-saved GPRs (software-saved on switch)
    //   [1]  r5
    //   [2]  r6
    //   [3]  r7
    //   [4]  r8
    //   [5]  r9
    //   [6]  r10
    //   [7]  r11
    //   [8]  cpsr    - SYS mode, I-bit clear (interrupts enabled), T-bit 0 (ARM)
    //   [9]  r0      - entry_arg (first argument to task entry function)
    //   [10] r1      - 0
    //   [11] r2      - 0
    //   [12] r3      - 0
    //   [13] r12     - 0
    //   [14] lr      - xRTOS_Task_Exit (task exit on function return)
    //   [15] pc      - task entry function address
    //
    // stack_top is set to &frame[0] after this frame is built.
    typedef struct xRTOS_Port_ARM_R5_Frame_t
    {
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t r8;
        uint32_t r9;
        uint32_t r10;
        uint32_t r11;
        uint32_t cpsr;
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r12;
        uint32_t lr;
        uint32_t pc;
    } xRTOS_Port_ARM_R5_Frame_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // Port ops table for ARM Cortex-R5. Pass to xRTOS_Kernel_Init.
    extern const xRTOS_Port_Ops_t xrtos_arm_r5_port_ops;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    void xRTOS_Port_ARM_R5_Pre_Switch(void);
    void xrtos_port_arm_r5_irq_handler(void);
    bool xrtos_port_arm_r5_should_switch(void);

    // Implemented in xrtos_port_arm_r5_asm.S.
    // Saves the current task context onto its stack, selects the next task
    // identified by kernel->scheduler.current_priority, and restores its context.
    // Callable only from the context-switch interrupt handler (SVC / SGI).
    void xRTOS_Port_ARM_R5_Context_Switch(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PORT_ARM_R5_H
// EOF /////////////////////////////////////////////////////////////////////////////
