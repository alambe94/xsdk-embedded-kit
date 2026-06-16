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

// @file xrtos_port_am243x.c
// @brief Standalone AM243x Cortex-R5 IRQ and tick integration for xRTOS.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xrtos_port_am243x.h"
#include "xrtos_tick.h"

// AM243x VIM register offsets. The default base is for R5FSS0-0 and can be
// overridden by xRTOS_Port_AM243x_Set_VIM_Base during early board init.
#define AM243X_VIM_IRQVEC      0x18U
#define AM243X_VIM_FIQVEC      0x1CU
#define AM243X_VIM_ACTIRQ      0x20U
#define AM243X_VIM_RAW(j)      (0x400U + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_STS(j)      (0x404U + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_INT_EN(j)   (0x408U + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_INT_DIS(j)  (0x40CU + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_INT_MAP(j)  (0x418U + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_INT_TYPE(j) (0x41CU + ((((j) >> 5U) & 0xFU) * 0x20U))
#define AM243X_VIM_INT_PRI(j)  (0x1000U + ((j) * 0x4U))
#define AM243X_VIM_INT_VEC(j)  (0x2000U + ((j) * 0x4U))

#define AM243X_VIM_ACTIVE_VALID 0x80000000U
#define AM243X_VIM_INT_MASK     (XRTOS_PORT_AM243X_MAX_INTERRUPTS - 1U)
#define AM243X_VIM_BANK_COUNT   (XRTOS_PORT_AM243X_MAX_INTERRUPTS / 32U)

typedef struct xRTOS_Port_AM243x_IRQ_Entry_t
{
    xRTOS_Port_AM243x_ISR_t isr;
    void *args;
} xRTOS_Port_AM243x_IRQ_Entry_t;

static uintptr_t s_vim_base;
static xRTOS_Port_AM243x_IRQ_Entry_t s_irq_table[XRTOS_PORT_AM243X_MAX_INTERRUPTS];
static uint32_t s_irq_is_pulse[AM243X_VIM_BANK_COUNT];
static uint32_t s_spurious_irq_count;

extern void xRTOS_Port_ARM_R5_IRQ_Handler(void);

static volatile uint32_t *am243x_vim_reg(uint32_t offset)
{
    uintptr_t vim_base = (s_vim_base != 0U) ? s_vim_base : XRTOS_PORT_AM243X_VIM_BASE_DEFAULT;
    return (volatile uint32_t *)(vim_base + (uintptr_t)offset);
}

static uint32_t am243x_irq_bit(uint32_t int_num)
{
    return 1UL << (int_num & 0x1FU);
}

static uint32_t am243x_irq_bank(uint32_t int_num)
{
    return int_num >> 5U;
}

static void am243x_barrier(void)
{
    __asm volatile("dsb\n"
                   "isb\n"
                   :
                   :
                   : "memory");
}

static bool am243x_irq_is_valid(uint32_t int_num)
{
    return int_num < XRTOS_PORT_AM243X_MAX_INTERRUPTS;
}

static bool am243x_decode_active_irq(uint32_t active_irq, uint32_t *int_num)
{
    uint32_t decoded_int_num = active_irq & AM243X_VIM_INT_MASK;
    if (!am243x_irq_is_valid(decoded_int_num))
    {
        return false;
    }

    *int_num = decoded_int_num;
    if ((active_irq & AM243X_VIM_ACTIVE_VALID) != 0U)
    {
        return true;
    }

    return (*am243x_vim_reg(AM243X_VIM_STS(decoded_int_num)) & am243x_irq_bit(decoded_int_num)) != 0U;
}

__attribute__((weak)) void xRTOS_Port_AM243x_Early_Init(void)
{
}

void xRTOS_Port_AM243x_Set_VIM_Base(uintptr_t vim_base)
{
    if (vim_base != 0U)
    {
        s_vim_base = vim_base;
    }
}

void xRTOS_Port_AM243x_Init(void)
{
    if (s_vim_base == 0U)
    {
        s_vim_base = XRTOS_PORT_AM243X_VIM_BASE_DEFAULT;
    }

    for (uint32_t int_num = 0U; int_num < XRTOS_PORT_AM243X_MAX_INTERRUPTS; int_num++)
    {
        s_irq_table[int_num].isr = NULL;
        s_irq_table[int_num].args = NULL;
        *am243x_vim_reg(AM243X_VIM_INT_PRI(int_num)) = 0xFU;
        *am243x_vim_reg(AM243X_VIM_INT_VEC(int_num)) = (uint32_t)(uintptr_t)xRTOS_Port_ARM_R5_IRQ_Handler;
    }

    for (uint32_t bank = 0U; bank < AM243X_VIM_BANK_COUNT; bank++)
    {
        uint32_t first_int = bank * 32U;
        *am243x_vim_reg(AM243X_VIM_INT_DIS(first_int)) = 0xFFFFFFFFU;
        *am243x_vim_reg(AM243X_VIM_STS(first_int)) = 0xFFFFFFFFU;
        *am243x_vim_reg(AM243X_VIM_INT_TYPE(first_int)) = 0U;
        *am243x_vim_reg(AM243X_VIM_INT_MAP(first_int)) = 0U;
        s_irq_is_pulse[bank] = 0U;
    }

    *am243x_vim_reg(AM243X_VIM_IRQVEC) = 0U;
    *am243x_vim_reg(AM243X_VIM_FIQVEC) = 0U;
    s_spurious_irq_count = 0U;
    am243x_barrier();
}

bool xRTOS_Port_AM243x_Register_IRQ(uint32_t int_num, xRTOS_Port_AM243x_ISR_t isr, void *args, uint32_t priority, bool is_pulse)
{
    if (!am243x_irq_is_valid(int_num) || (isr == NULL) || (priority > 0xFU))
    {
        return false;
    }

    xRTOS_Port_AM243x_Disable_IRQ(int_num);
    xRTOS_Port_AM243x_Clear_IRQ(int_num);

    uint32_t bit = am243x_irq_bit(int_num);
    uint32_t bank = am243x_irq_bank(int_num);

    *am243x_vim_reg(AM243X_VIM_INT_MAP(int_num)) &= ~bit;
    *am243x_vim_reg(AM243X_VIM_INT_PRI(int_num)) = priority & 0xFU;
    *am243x_vim_reg(AM243X_VIM_INT_VEC(int_num)) = (uint32_t)(uintptr_t)xRTOS_Port_ARM_R5_IRQ_Handler;

    if (is_pulse)
    {
        *am243x_vim_reg(AM243X_VIM_INT_TYPE(int_num)) |= bit;
        s_irq_is_pulse[bank] |= bit;
    }
    else
    {
        *am243x_vim_reg(AM243X_VIM_INT_TYPE(int_num)) &= ~bit;
        s_irq_is_pulse[bank] &= ~bit;
    }

    s_irq_table[int_num].isr = isr;
    s_irq_table[int_num].args = args;

    xRTOS_Port_AM243x_Enable_IRQ(int_num);
    return true;
}

void xRTOS_Port_AM243x_Enable_IRQ(uint32_t int_num)
{
    if (am243x_irq_is_valid(int_num))
    {
        *am243x_vim_reg(AM243X_VIM_INT_EN(int_num)) = am243x_irq_bit(int_num);
        am243x_barrier();
    }
}

void xRTOS_Port_AM243x_Disable_IRQ(uint32_t int_num)
{
    if (am243x_irq_is_valid(int_num))
    {
        *am243x_vim_reg(AM243X_VIM_INT_DIS(int_num)) = am243x_irq_bit(int_num);
        am243x_barrier();
    }
}

void xRTOS_Port_AM243x_Clear_IRQ(uint32_t int_num)
{
    if (am243x_irq_is_valid(int_num))
    {
        *am243x_vim_reg(AM243X_VIM_STS(int_num)) = am243x_irq_bit(int_num);
    }
}

uint32_t xRTOS_Port_AM243x_Get_Spurious_IRQ_Count(void)
{
    return s_spurious_irq_count;
}

void xRTOS_Port_AM243x_Tick_ISR(void *args)
{
    (void)args;

    bool should_yield = false;
    xRTOS_Tick_Increment_From_ISR(&should_yield);

    (void)should_yield;
}

void xrtos_port_arm_r5_irq_handler(void)
{
    uint32_t active_irq = *am243x_vim_reg(AM243X_VIM_ACTIRQ);
    uint32_t int_num;

    if (!am243x_decode_active_irq(active_irq, &int_num))
    {
        s_spurious_irq_count++;
        *am243x_vim_reg(AM243X_VIM_IRQVEC) = 0U;
        return;
    }

    uint32_t bit = am243x_irq_bit(int_num);
    uint32_t bank = am243x_irq_bank(int_num);
    bool is_pulse = (s_irq_is_pulse[bank] & bit) != 0U;

    if (is_pulse)
    {
        xRTOS_Port_AM243x_Clear_IRQ(int_num);
    }

    xRTOS_Port_AM243x_ISR_t isr = s_irq_table[int_num].isr;
    if (isr != NULL)
    {
        isr(s_irq_table[int_num].args);
    }

    if (!is_pulse)
    {
        xRTOS_Port_AM243x_Clear_IRQ(int_num);
    }

    *am243x_vim_reg(AM243X_VIM_IRQVEC) = int_num;
}

// EOF /////////////////////////////////////////////////////////////////////////////
