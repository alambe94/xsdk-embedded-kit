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

// @file xrtos_port_qemu_r5.c
// @brief QEMU Cortex-R5 GIC and Private Timer interrupt handling for xRTOS.

#include <stdbool.h>
#include <stdint.h>

#include "xrtos_core.h"
#include "xrtos_tick.h"
#include "xrtos_private.h"

#define GIC_CPU_BASE       0xF8F00100U
#define PRIVATE_TIMER_BASE 0xF8F00600U

#define GICC_IAR  (*(volatile uint32_t *)(GIC_CPU_BASE + 0x0CU))
#define GICC_EOIR (*(volatile uint32_t *)(GIC_CPU_BASE + 0x10U))
#define PT_ISR    (*(volatile uint32_t *)(PRIVATE_TIMER_BASE + 0x0CU))

void xrtos_port_arm_r5_irq_handler(void)
{
    uint32_t iar = GICC_IAR;
    uint32_t interrupt_id = iar & 0x3FFU;
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_ISR_ENTER, interrupt_id);

    if (interrupt_id == 29U)
    {
        PT_ISR = 1U;
        bool should_yield = false;
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    xRTOS_TRACE_E1(kernel, xRTOS_TRACE_CODE_ISR_EXIT, interrupt_id);

    GICC_EOIR = iar;
}

// EOF /////////////////////////////////////////////////////////////////////////////

