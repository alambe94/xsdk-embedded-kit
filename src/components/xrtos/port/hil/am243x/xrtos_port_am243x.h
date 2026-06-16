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

// @file xrtos_port_am243x.h
// @brief Standalone AM243x Cortex-R5 target glue for xRTOS.

#ifndef XRTOS_PORT_AM243X_H
#define XRTOS_PORT_AM243X_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

#ifndef XRTOS_PORT_AM243X_MAX_INTERRUPTS
#define XRTOS_PORT_AM243X_MAX_INTERRUPTS 512U
#endif

#ifndef XRTOS_PORT_AM243X_VIM_BASE_DEFAULT
#define XRTOS_PORT_AM243X_VIM_BASE_DEFAULT 0x2FFF0000U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef void (*xRTOS_Port_AM243x_ISR_t)(void *args);

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Optional hook called by the standalone reset handler before main().
    void xRTOS_Port_AM243x_Early_Init(void);

    // Initialize the local AM243x VIM interrupt table and mask all VIM inputs.
    void xRTOS_Port_AM243x_Init(void);

    // Override the default VIM base when targeting another R5F core/VIM view.
    void xRTOS_Port_AM243x_Set_VIM_Base(uintptr_t vim_base);

    // Register a VIM IRQ line with the local xRTOS AM243x dispatcher.
    bool xRTOS_Port_AM243x_Register_IRQ(uint32_t int_num, xRTOS_Port_AM243x_ISR_t isr, void *args, uint32_t priority, bool is_pulse);

    void xRTOS_Port_AM243x_Enable_IRQ(uint32_t int_num);
    void xRTOS_Port_AM243x_Disable_IRQ(uint32_t int_num);
    void xRTOS_Port_AM243x_Clear_IRQ(uint32_t int_num);

    uint32_t xRTOS_Port_AM243x_Get_Spurious_IRQ_Count(void);

    // Register this function as the callback for the xRTOS system tick IRQ.
    void xRTOS_Port_AM243x_Tick_ISR(void *args);

    // Strong override for the weak generic ARM R5 IRQ callback. It is called by
    // xRTOS_Port_ARM_R5_IRQ_Handler after the interrupted task context is saved.
    void xrtos_port_arm_r5_irq_handler(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XRTOS_PORT_AM243X_H

// EOF /////////////////////////////////////////////////////////////////////////////
