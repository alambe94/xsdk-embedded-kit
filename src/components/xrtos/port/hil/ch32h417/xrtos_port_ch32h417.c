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

// @file xrtos_port_ch32h417.c
// @brief xRTOS CH32H417 QingKeV5F port scaffold.

#include "xrtos_port_ch32h417.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xrtos_core.h"
#include "xrtos_private.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"

#define XRTOS_CH32H417_MSTATUS_MIE 0x00000008U
#define XRTOS_CH32H417_TASK_MSTATUS 0x00006080U
#define XRTOS_CH32H417_SOFTWARE_IRQ 14U
#define XRTOS_CH32H417_SOFTWARE_IRQ_BIT (1UL << XRTOS_CH32H417_SOFTWARE_IRQ)
#define XRTOS_CH32H417_SOFTWARE_IRQ_PRIORITY 0xC0U

#define XRTOS_CH32H417_PFIC_IENR1    (*(volatile uint32_t *)0xE000E100U)
#define XRTOS_CH32H417_PFIC_IPSR1    (*(volatile uint32_t *)0xE000E200U)
#define XRTOS_CH32H417_PFIC_IPRR1    (*(volatile uint32_t *)0xE000E280U)
#define XRTOS_CH32H417_PFIC_IPRIOR14 (*(volatile uint8_t *)0xE000E40EU)
#define XRTOS_CH32H417_ISR_STACK_WORDS 128U

#define XRTOS_CH32H417_SYSTICK1_CTLR (*(volatile uint32_t *)0xE000F080U)
#define XRTOS_CH32H417_SYSTICK1_CNT  (*(volatile uint32_t *)0xE000F088U)
#define XRTOS_CH32H417_SYSTICK1_CMP  (*(volatile uint32_t *)0xE000F090U)
#define XRTOS_CH32H417_SYSTICK_ISR   (*(volatile uint32_t *)0xE000F004U)
#define XRTOS_CH32H417_PFIC_IPRIOR13 (*(volatile uint8_t *)0xE000E40DU)

#define XRTOS_CH32H417_SYSTICK1_IRQ       13U
#define XRTOS_CH32H417_SYSTICK1_IRQ_BIT   (1UL << XRTOS_CH32H417_SYSTICK1_IRQ)
#define XRTOS_CH32H417_SYSTICK1_ISR_BIT   (1UL << 1)
#define XRTOS_CH32H417_SYSTICK1_PRIORITY  0x80U
#define XRTOS_CH32H417_SYSTICK1_CTLR_RUN  0x0FU

static volatile uint32_t xRTOS_CH32H417_ISR_Nesting;
uint32_t xRTOS_CH32H417_ISR_Stack[XRTOS_CH32H417_ISR_STACK_WORDS] __attribute__((aligned(16)));

static void ch32h417_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg);
static void ch32h417_start_first_task(xRTOS_Task_Context_t *task_ctx);
static void ch32h417_yield(void);
static uint32_t ch32h417_disable_interrupts(void);
static void ch32h417_enable_interrupts(uint32_t saved_state);
static bool ch32h417_is_in_isr(void);
static void ch32h417_configure_software_irq(void);
static void ch32h417_zero_frame(xRTOS_Port_CH32H417_Frame_t *frame);

const xRTOS_Port_Ops_t xrtos_ch32h417_port_ops = {
    .init_task_stack = ch32h417_init_task_stack,
    .start_first_task = ch32h417_start_first_task,
    .yield = ch32h417_yield,
    .disable_interrupts = ch32h417_disable_interrupts,
    .enable_interrupts = ch32h417_enable_interrupts,
    .is_in_isr = ch32h417_is_in_isr,
};

static void ch32h417_zero_frame(xRTOS_Port_CH32H417_Frame_t *frame)
{
    uint32_t *word = (uint32_t *)frame;

    for (uint32_t i = 0U; i < (sizeof(*frame) / sizeof(uint32_t)); i++)
    {
        word[i] = 0U;
    }
}

static void ch32h417_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg)
{
    if ((task_ctx == NULL) || (entry == NULL) || (task_ctx->stack_mem == NULL) ||
        (task_ctx->stack_words < XRTOS_PORT_CH32H417_MIN_STACK_WORDS))
    {
        return;
    }

    uintptr_t top_addr = (uintptr_t)(task_ctx->stack_mem + task_ctx->stack_words);
    top_addr &= ~((uintptr_t)(XRTOS_PORT_CH32H417_STACK_ALIGN_BYTES - 1U));

    xRTOS_Port_CH32H417_Frame_t *frame = ((xRTOS_Port_CH32H417_Frame_t *)top_addr) - 1;
    ch32h417_zero_frame(frame);

    frame->mepc = (uint32_t)(uintptr_t)entry;
    frame->ra = (uint32_t)(uintptr_t)xRTOS_Task_Exit;
    frame->a0 = (uint32_t)(uintptr_t)arg;
    frame->mstatus = XRTOS_CH32H417_TASK_MSTATUS;

    task_ctx->stack_top = (uint32_t *)frame;
}

static void ch32h417_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    if ((task_ctx == NULL) || (task_ctx->stack_top == NULL))
    {
        return;
    }

    ch32h417_configure_software_irq();
    xRTOS_Port_CH32H417_Start_First_Task(task_ctx);
}

static void ch32h417_yield(void)
{
    XRTOS_CH32H417_PFIC_IPSR1 = XRTOS_CH32H417_SOFTWARE_IRQ_BIT;
}

static uint32_t ch32h417_disable_interrupts(void)
{
    uint32_t saved_state;
    __asm volatile("csrrc %0, 0x800, %1" : "=r"(saved_state) : "r"(XRTOS_CH32H417_MSTATUS_MIE) : "memory");
    return saved_state;
}

static void ch32h417_enable_interrupts(uint32_t saved_state)
{
    __asm volatile("csrw 0x800, %0" : : "r"(saved_state) : "memory");
}

static bool ch32h417_is_in_isr(void)
{
    return xRTOS_CH32H417_ISR_Nesting != 0U;
}

static void ch32h417_configure_software_irq(void)
{
    XRTOS_CH32H417_PFIC_IPRR1 = XRTOS_CH32H417_SOFTWARE_IRQ_BIT;
    XRTOS_CH32H417_PFIC_IPRIOR14 = XRTOS_CH32H417_SOFTWARE_IRQ_PRIORITY;
    XRTOS_CH32H417_PFIC_IENR1 = XRTOS_CH32H417_SOFTWARE_IRQ_BIT;
}

void xRTOS_Port_CH32H417_Tick_ISR(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();
    if ((kernel == NULL) || (!kernel->is_initialized))
    {
        return;
    }

    bool should_yield = false;

    xRTOS_CH32H417_ISR_Nesting++;
    xRTOS_Tick_Increment_From_ISR(&should_yield);
    xRTOS_CH32H417_ISR_Nesting--;

    if (should_yield)
    {
        ch32h417_yield();
    }
}

void xRTOS_Port_CH32H417_Switch_Context(void)
{
    xRTOS_Kernel_Context_t *kernel = xrtos_kernel_get();

    xRTOS_CH32H417_ISR_Nesting++;

    if ((kernel != NULL) && kernel->is_initialized && kernel->scheduler.is_started && kernel->scheduler.is_schedule_pending)
    {
        xRTOS_Task_Context_t *current = xRTOS_Scheduler_Current_Task();
        if ((current != NULL) && (current->state == xRTOS_TASK_STATE_RUNNING))
        {
            current->state = xRTOS_TASK_STATE_READY;
            xrtos_scheduler_ready_add(kernel, current->task_id);
        }

        if (xRTOS_Scheduler_Select_Next() == xRETURN_xRTOS_OK)
        {
            xRTOS_Scheduler_Switch();
        }
    }

    xRTOS_CH32H417_ISR_Nesting--;
}

void xRTOS_Port_CH32H417_Timer_Init(uint32_t hclk_hz, uint32_t tick_hz)
{
    uint32_t cycles_per_tick;

    if ((hclk_hz == 0U) || (tick_hz == 0U))
    {
        return;
    }

    cycles_per_tick = hclk_hz / tick_hz;
    if (cycles_per_tick == 0U)
    {
        return;
    }

    XRTOS_CH32H417_SYSTICK1_CTLR = 0U;
    XRTOS_CH32H417_SYSTICK1_CNT = 0U;
    XRTOS_CH32H417_SYSTICK1_CMP = cycles_per_tick - 1U;

    XRTOS_CH32H417_SYSTICK_ISR &= ~XRTOS_CH32H417_SYSTICK1_ISR_BIT;
    XRTOS_CH32H417_PFIC_IPRR1 = XRTOS_CH32H417_SYSTICK1_IRQ_BIT;
    XRTOS_CH32H417_PFIC_IPRIOR13 = XRTOS_CH32H417_SYSTICK1_PRIORITY;
    XRTOS_CH32H417_PFIC_IENR1 = XRTOS_CH32H417_SYSTICK1_IRQ_BIT;
    XRTOS_CH32H417_SYSTICK1_CTLR = XRTOS_CH32H417_SYSTICK1_CTLR_RUN;
}

void SysTick1_Handler(void) __attribute__((interrupt("machine")));

void SysTick1_Handler(void)
{
    XRTOS_CH32H417_SYSTICK_ISR &= ~XRTOS_CH32H417_SYSTICK1_ISR_BIT;
    xRTOS_Port_CH32H417_Tick_ISR();
    XRTOS_CH32H417_PFIC_IPRR1 = XRTOS_CH32H417_SYSTICK1_IRQ_BIT;
}

void Ecall_M_Mode_Handler(void) __attribute__((interrupt("machine")));

void Ecall_M_Mode_Handler(void)
{
    uint32_t mepc;

    __asm volatile("csrr %0, mepc" : "=r"(mepc));
    mepc += 4U;
    __asm volatile("csrw mepc, %0" : : "r"(mepc) : "memory");
}

// EOF /////////////////////////////////////////////////////////////////////////////
