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

// @file test_perfetto_showcase.c
// @brief xRTOS QEMU Perfetto showcase: producer-consumer pattern + spinners.
//
// Task / priority layout
// -------------------------------------------------------------
//  task_id   priority  role
//     0         0      supervisor
//     1         1      producer  (signals sem_a / sem_b alternately)
//     2         2      consumer_a (takes from sem_a)
//     3         2      consumer_b (takes from sem_b)
//     4         3      spinner_a  (busy-loop)
//     5         3      spinner_b  (busy-loop)
//    30         1      timer daemon (auto-created by Kernel_Init)
//    31        31      idle
// -------------------------------------------------------------
//
// The producer runs for 60 ticks (20 iterations x 3 ticks each), alternating
// between sem_a and sem_b.  Consumers block when their semaphore is empty and
// wake when the producer signals.  Spinners (equal priority) round-robin for
// the entire test duration.
//
// The supervisor sleeps 80 ticks then verifies:
//   * each consumer received >= 8 tokens, and
//   * each spinner ran at least once.
//
// When built with QEMU_TRACE_ENABLED the scheduler events are streamed to
// UART and can be decoded by xtrace_decoder.py + perfetto_proto_exporter.py
// to produce a Perfetto proto trace.

#include <stdbool.h>
#include <stdint.h>

void xassert_system_halt(void);
#define xASSERT_HOOK() xassert_system_halt()

#include "xrtos_core.h"
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_arm_r5.h"
#include "uart.h"

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
#include "xtrace.h"

#define TRACE_BUF_BYTES 65536U
static uint8_t s_trace_buf[TRACE_BUF_BYTES];
static xTRACE_Context_t s_trace_ctx;

static xRETURN_t trace_write(void *ctx, const uint8_t *buf, size_t len, size_t *written)
{
    (void)ctx;
    for (uint32_t i = 0U; i < len; i++)
    {
        uart_trace_putb(buf[i]);
    }
    *written = len;
    return xRETURN_OK;
}
static const xTRACE_Transport_t s_trace_transport = {.write = trace_write};

#ifdef BOARD_REALVIEW_PB_A8
#define TRACE_TIMER2_VALUE (*(volatile uint32_t *)0x10011024U)
static xTRACE_Time_t trace_us_fn(void *ctx)
{
    (void)ctx;
    return (xTRACE_Time_t)(0xFFFFFFFFU - TRACE_TIMER2_VALUE);
}
#define TRACE_TIMESTAMP_FN trace_us_fn
#define TRACE_TIMESTAMP_HZ 1000000U
#else
static xTRACE_Time_t rtos_tick_fn(void *ctx)
{
    (void)ctx;
    return (xTRACE_Time_t)xRTOS_Tick_Get();
}
#define TRACE_TIMESTAMP_FN rtos_tick_fn
#define TRACE_TIMESTAMP_HZ 100U
#endif
#endif // QEMU_TRACE_ENABLED

// -- Kernel and task storage ------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;

static xRTOS_Task_Context_t s_supervisor_ctx;
static xRTOS_Task_Context_t s_producer_ctx;
static xRTOS_Task_Context_t s_consumer_a_ctx;
static xRTOS_Task_Context_t s_consumer_b_ctx;
static xRTOS_Task_Context_t s_spinner_a_ctx;
static xRTOS_Task_Context_t s_spinner_b_ctx;
static xRTOS_Task_Context_t s_idle_ctx;

static uint32_t s_supervisor_stack[256];
static uint32_t s_producer_stack[128];
static uint32_t s_consumer_a_stack[128];
static uint32_t s_consumer_b_stack[128];
static uint32_t s_spinner_a_stack[128];
static uint32_t s_spinner_b_stack[128];
static uint32_t s_idle_stack[128];

// -- Semaphores and shared state --------------------------------------------

static xRTOS_Sem_Context_t s_sem_a;
static xRTOS_Sem_Context_t s_sem_b;

static volatile uint32_t s_consumer_a_count;
static volatile uint32_t s_consumer_b_count;
static volatile uint32_t s_spinner_a_count;
static volatile uint32_t s_spinner_b_count;
static volatile bool s_stop;

// -- assert hook -----------------------------------------------------------

void xassert_system_halt(void)
{
#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xTRACE_Flush(&s_trace_ctx);
#endif
    while (1)
    {
    }
}

// -- Task implementations ---------------------------------------------------

static void producer_entry(void *arg)
{
    (void)arg;
    for (uint32_t i = 0U; i < 20U; i++)
    {
        (void)xRTOS_Task_Delay(3U);
        if ((i & 1U) == 0U)
        {
            (void)xRTOS_Sem_Give(&s_sem_a);
        }
        else
        {
            (void)xRTOS_Sem_Give(&s_sem_b);
        }
    }
    s_stop = true;
    xRTOS_Task_Exit();
}

static void consumer_a_entry(void *arg)
{
    (void)arg;
    while (!s_stop)
    {
        xRETURN_t ret = xRTOS_Sem_Take(&s_sem_a, 4U);
        if (ret == xRETURN_xRTOS_OK)
        {
            s_consumer_a_count++;
        }
    }
    xRTOS_Task_Exit();
}

static void consumer_b_entry(void *arg)
{
    (void)arg;
    while (!s_stop)
    {
        xRETURN_t ret = xRTOS_Sem_Take(&s_sem_b, 4U);
        if (ret == xRETURN_xRTOS_OK)
        {
            s_consumer_b_count++;
        }
    }
    xRTOS_Task_Exit();
}

static void spinner_entry(void *arg)
{
    volatile uint32_t *count = (volatile uint32_t *)arg;
    while (!s_stop)
    {
        (*count)++;
    }
    xRTOS_Task_Exit();
}

static void supervisor_entry(void *arg)
{
    (void)arg;
    uart_puts("\nPerfetto Showcase Test\n");
    uart_puts("Producer + 2 consumers + 2 spinners. Sleeping 80 ticks...\n");

    (void)xRTOS_Task_Delay(80U);

    // Extra ticks so every task can observe s_stop and exit cleanly.
    (void)xRTOS_Task_Delay(8U);

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xTRACE_Flush(&s_trace_ctx);
#endif

    uart_puts("consumer_a=");
    uart_puti(s_consumer_a_count);
    uart_puts(" consumer_b=");
    uart_puti(s_consumer_b_count);
    uart_puts("\n");
    uart_puts("spinner_a=");
    uart_puti(s_spinner_a_count);
    uart_puts(" spinner_b=");
    uart_puti(s_spinner_b_count);
    uart_puts("\n");

    uart_puts("\n=======================================================\n");
    if ((s_consumer_a_count >= 8U) && (s_consumer_b_count >= 8U) && (s_spinner_a_count > 0U) && (s_spinner_b_count > 0U))
    {
        uart_puts("PERF_SHOW PASS\n");
    }
    else
    {
        uart_puts("PERF_SHOW FAIL\n");
    }
    uart_puts("=======================================================\n");

    xRTOS_Task_Exit();
}

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
    }
}

// -- Hardware init (REALVIEW_PB_A8) -----------------------------------------

#ifdef BOARD_REALVIEW_PB_A8

#define GIC_CPU_BASE     0x1E000000U
#define GIC_DIST_BASE    0x1E001000U
#define SP804_TIMER_BASE 0x10011000U

#define GICC_CTLR (*(volatile uint32_t *)(GIC_CPU_BASE + 0x00U))
#define GICC_PMR  (*(volatile uint32_t *)(GIC_CPU_BASE + 0x04U))
#define GICC_IAR  (*(volatile uint32_t *)(GIC_CPU_BASE + 0x0CU))
#define GICC_EOIR (*(volatile uint32_t *)(GIC_CPU_BASE + 0x10U))

#define GICD_CTLR          (*(volatile uint32_t *)(GIC_DIST_BASE + 0x000U))
#define GICD_ISENABLER(n)  (((volatile uint32_t *)(GIC_DIST_BASE + 0x100U))[n])
#define GICD_IPRIORITYR(n) (((volatile uint32_t *)(GIC_DIST_BASE + 0x400U))[n])

#define Timer1Load    (*(volatile uint32_t *)(SP804_TIMER_BASE + 0x00U))
#define Timer1Control (*(volatile uint32_t *)(SP804_TIMER_BASE + 0x08U))
#define Timer1IntClr  (*(volatile uint32_t *)(SP804_TIMER_BASE + 0x0CU))

static void hardware_init(void)
{
    GICC_PMR = 0xFFU;
    GICC_CTLR = 0x1U;

    GICD_ISENABLER(1) = (1U << 4U);

    uint32_t prio = GICD_IPRIORITYR(9);
    prio &= ~0xFFU;
    prio |= 0xA0U;
    GICD_IPRIORITYR(9) = prio;

    GICD_CTLR = 0x1U;

    Timer1Load = 10000U;
    Timer1IntClr = 1U;
    Timer1Control = 0xE2U;

#define Timer2Control (*(volatile uint32_t *)(SP804_TIMER_BASE + 0x28U))
    Timer2Control = 0x82U; // free-running 1 MHz for xTRACE timestamps
}

void xrtos_port_arm_r5_irq_handler(void)
{
    uint32_t iar = GICC_IAR;
    uint32_t interrupt_id = iar & 0x3FFU;

    xRTOS_TRACE_E1(&s_kernel, xRTOS_TRACE_CODE_ISR_ENTER, interrupt_id);

    if (interrupt_id == 36U)
    {
        Timer1IntClr = 1U;
        bool should_yield = false;
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    xRTOS_TRACE_E1(&s_kernel, xRTOS_TRACE_CODE_ISR_EXIT, interrupt_id);

    GICC_EOIR = iar;
}

#endif // BOARD_REALVIEW_PB_A8

// -- main ------------------------------------------------------------------

int main(void)
{
    uart_init();
    uart_puts("\nxRTOS Perfetto Showcase QEMU Test\n");

    xRETURN_t ret = xRTOS_Kernel_Init(&s_kernel, &xrtos_arm_r5_port_ops);
    if (ret != xRETURN_xRTOS_OK)
    {
        uart_puts("Kernel init FAILED\n");
        return 1;
    }

    hardware_init();

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    {
        xTRACE_Config_t tcfg = {.buffer = s_trace_buf,
                                .capacity_bytes = TRACE_BUF_BYTES,
                                .timestamp_fn = TRACE_TIMESTAMP_FN,
                                .timestamp_ctx = NULL,
                                .timestamp_hz = TRACE_TIMESTAMP_HZ,
                                .is_enabled = true};
        (void)xTRACE_Init(&s_trace_ctx, &tcfg, &s_trace_transport, NULL);
        (void)xRTOS_Kernel_Trace_Init(&s_trace_ctx);
    }
#endif

    (void)xRTOS_Sem_Init(&s_sem_a, 0U, 1U, "SemA");
    (void)xRTOS_Sem_Init(&s_sem_b, 0U, 1U, "SemB");

    (void)xRTOS_Task_Create(&s_idle_ctx, &(xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                                                .priority = xRTOS_IDLE_PRIORITY,
                                                                .entry = idle_entry,
                                                                .entry_arg = NULL,
                                                                .stack_mem = s_idle_stack,
                                                                .stack_words = 128U,
                                                                .name = "Idle"});

    (void)xRTOS_Task_Create(&s_supervisor_ctx, &(xRTOS_Task_Config_t){.task_id = 0U,
                                                                      .priority = 0U,
                                                                      .entry = supervisor_entry,
                                                                      .entry_arg = NULL,
                                                                      .stack_mem = s_supervisor_stack,
                                                                      .stack_words = 256U,
                                                                      .name = "Supervisor"});

    (void)xRTOS_Task_Create(&s_producer_ctx, &(xRTOS_Task_Config_t){.task_id = 1U,
                                                                    .priority = 1U,
                                                                    .entry = producer_entry,
                                                                    .entry_arg = NULL,
                                                                    .stack_mem = s_producer_stack,
                                                                    .stack_words = 128U,
                                                                    .name = "Producer"});

    (void)xRTOS_Task_Create(&s_consumer_a_ctx, &(xRTOS_Task_Config_t){.task_id = 2U,
                                                                      .priority = 2U,
                                                                      .entry = consumer_a_entry,
                                                                      .entry_arg = NULL,
                                                                      .stack_mem = s_consumer_a_stack,
                                                                      .stack_words = 128U,
                                                                      .name = "ConsumerA"});

    (void)xRTOS_Task_Create(&s_consumer_b_ctx, &(xRTOS_Task_Config_t){.task_id = 3U,
                                                                      .priority = 2U,
                                                                      .entry = consumer_b_entry,
                                                                      .entry_arg = NULL,
                                                                      .stack_mem = s_consumer_b_stack,
                                                                      .stack_words = 128U,
                                                                      .name = "ConsumerB"});

    (void)xRTOS_Task_Create(&s_spinner_a_ctx, &(xRTOS_Task_Config_t){.task_id = 4U,
                                                                     .priority = 3U,
                                                                     .entry = spinner_entry,
                                                                     .entry_arg = (void *)&s_spinner_a_count,
                                                                     .stack_mem = s_spinner_a_stack,
                                                                     .stack_words = 128U,
                                                                     .name = "SpinnerA"});

    (void)xRTOS_Task_Create(&s_spinner_b_ctx, &(xRTOS_Task_Config_t){.task_id = 5U,
                                                                     .priority = 3U,
                                                                     .entry = spinner_entry,
                                                                     .entry_arg = (void *)&s_spinner_b_count,
                                                                     .stack_mem = s_spinner_b_stack,
                                                                     .stack_words = 128U,
                                                                     .name = "SpinnerB"});

    uart_puts("Starting scheduler...\n");
    xRTOS_Kernel_Start();

    return 0;
}

// EOF /////////////////////////////////////////////////////////////////////////////
