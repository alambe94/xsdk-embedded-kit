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

// @file test_rr32.c
// @brief xRTOS QEMU 32-slot round-robin verification test.
//
// Task / priority layout (all 32 task-ID slots occupied)
// -------------------------------------------------------------
//  task_id   priority  role
//     0         0      supervisor
//   1..29       2      workers (29-task round-robin group)
//    30         1      timer daemon (auto-created by Kernel_Init)
//    31        31      idle
// -------------------------------------------------------------
//
// All 29 workers share priority 2.  The supervisor sleeps for 120 ticks
// while the scheduler cycles workers 1->2->...->29->1... one slice per tick.
// On wake the supervisor checks:
//   * every counter is non-zero (each worker ran), and
//   * max_count < 2 x min_count (round-robin is fair).
// 120 ticks gives each worker 4-5 slices (ratio 1.25) which stays well
// within the 2x bound even when xTRACE ISR overhead distorts tick timing.
//
// xTRACE is streamed to UART1 when built with QEMU_TRACE_ENABLED.
// The resulting binary (.bin) can be decoded by xtrace_decoder.py to
// produce a Perfetto proto or Chrome Trace JSON showing every
// TASK_SWITCH event for all 29 workers.

#include <stdbool.h>
#include <stdint.h>

void xassert_system_halt(void);
#define xASSERT_HOOK() xassert_system_halt()

#include "xrtos_core.h"
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

#define RR32_WORKER_COUNT 29U // task IDs 1-29

static const char *const s_worker_names[RR32_WORKER_COUNT] = {
    "Worker1",  "Worker2",  "Worker3",  "Worker4",  "Worker5",  "Worker6",  "Worker7",  "Worker8",  "Worker9",  "Worker10",
    "Worker11", "Worker12", "Worker13", "Worker14", "Worker15", "Worker16", "Worker17", "Worker18", "Worker19", "Worker20",
    "Worker21", "Worker22", "Worker23", "Worker24", "Worker25", "Worker26", "Worker27", "Worker28", "Worker29",
};

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_supervisor_ctx;
static xRTOS_Task_Context_t s_worker_ctx[RR32_WORKER_COUNT];
static xRTOS_Task_Context_t s_idle_ctx;

static uint32_t s_supervisor_stack[256];
static uint32_t s_worker_stack[RR32_WORKER_COUNT][128];
static uint32_t s_idle_stack[128];

// -- Round-robin worker state -----------------------------------------------

static volatile uint32_t s_worker_count[RR32_WORKER_COUNT];
static volatile bool s_workers_stop;

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

static void worker_entry(void *arg)
{
    volatile uint32_t *count = (volatile uint32_t *)arg;
    while (!s_workers_stop)
    {
        (*count)++;
    }
    xRTOS_Task_Exit();
}

static void supervisor_entry(void *arg)
{
    (void)arg;

    uart_puts("\nxRTOS 32-Slot Round-Robin Test\n");
    uart_puts("Workers 1-29 at priority 2. Sleeping 120 ticks...\n");

    (void)xRTOS_Task_Delay(120U);

    s_workers_stop = true;
    (void)xRTOS_Task_Delay(8U); // allow every worker a chance to see the flag and exit

    uint32_t min_c = 0xFFFFFFFFU;
    uint32_t max_c = 0U;
    bool any_zero = false;

    for (uint32_t i = 0U; i < RR32_WORKER_COUNT; i++)
    {
        uint32_t c = s_worker_count[i];
        uart_puts("  worker ");
        uart_puti(1U + i);
        uart_puts(": ");
        uart_puti(c);
        uart_puts("\n");
        if (c == 0U)
        {
            any_zero = true;
        }
        if (c < min_c)
        {
            min_c = c;
        }
        if (c > max_c)
        {
            max_c = c;
        }
    }

    uart_puts("min=");
    uart_puti(min_c);
    uart_puts(" max=");
    uart_puti(max_c);
    uart_puts("\n");

#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xTRACE_Flush(&s_trace_ctx);
#endif

    uart_puts("\n=======================================================\n");
    if (!any_zero && (max_c < (min_c * 2U)))
    {
        uart_puts("RR32 PASS: 29-task round-robin balanced\n");
    }
    else
    {
        uart_puts("RR32 FAIL: worker counts not balanced\n");
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
    uart_puts("\nxRTOS RR32 QEMU Test\n");

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

    for (uint32_t i = 0U; i < RR32_WORKER_COUNT; i++)
    {
        (void)xRTOS_Task_Create(&s_worker_ctx[i], &(xRTOS_Task_Config_t){.task_id = 1U + i,
                                                                         .priority = 2U,
                                                                         .entry = worker_entry,
                                                                         .entry_arg = (void *)&s_worker_count[i],
                                                                         .stack_mem = s_worker_stack[i],
                                                                         .stack_words = 128U,
                                                                         .name = s_worker_names[i]});
    }

    uart_puts("Starting scheduler...\n");
    xRTOS_Kernel_Start();

    return 0;
}

// EOF /////////////////////////////////////////////////////////////////////////////
