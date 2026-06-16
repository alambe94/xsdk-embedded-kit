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

// @file test_xrtos_regression.c
// @brief xRTOS QEMU regression test: systematic coverage of all primitives.
//
// Task layout:
//   task_id 0  (prio 0)  supervisor - orchestrates all 8 phases
//   task_id 1  (varies)  helper 1   - recreated per phase
//   task_id 2  (varies)  helper 2   - recreated per phases that need it
//   task_id 3  (varies)  helper 3   - used only in phase 8 (round-robin)
//   task_id 31 (prio 31) idle
//
// Phase summary:
//   1 - counting semaphore: supervisor pre-loads 5 tokens, helper drains them
//   2 - semaphore timeout:  helper waits on empty semaphore, must time out
//   3 - mutex:              h1 holds mutex while h2 contends; verify h2 gets it
//   4 - event ANY:          h1 waits ANY(0x3); supervisor sets 0x1; verify wake
//   5 - event ALL:          h1 waits ALL(0x3); supervisor sets bits one by one
//   6 - queue FIFO:         h1 sends {1..5}; h2 receives and checks order
//   7 - task notify:        h1 waits notify; supervisor sends 0xBEEF
//   8 - round-robin:        3 equal-priority spinners; verify 2x fairness bound
//
// Pass string: "REGRESSION PASS"

#include <stdbool.h>
#include <stdint.h>

void xassert_system_halt(void);
#define xASSERT_HOOK() xassert_system_halt()

#include "xrtos_core.h"
#include "xrtos_event.h"
#include "xrtos_mutex.h"
#include "xrtos_notify.h"
#include "xrtos_queue.h"
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_arm_r5.h"
#include "uart.h"

// -- Kernel and persistent task storage ------------------------------------

static xRTOS_Kernel_Context_t s_kernel;

static xRTOS_Task_Context_t s_supervisor_ctx;
static xRTOS_Task_Context_t s_helper1_ctx;
static xRTOS_Task_Context_t s_helper2_ctx;
static xRTOS_Task_Context_t s_helper3_ctx;
static xRTOS_Task_Context_t s_idle_ctx;

static uint32_t s_supervisor_stack[384];
static uint32_t s_helper1_stack[256];
static uint32_t s_helper2_stack[256];
static uint32_t s_helper3_stack[256];
static uint32_t s_idle_stack[128];

// -- Per-phase primitives (reinitialized each phase) ------------------------

static xRTOS_Sem_Context_t s_sem;
static xRTOS_Mutex_Context_t s_mtx;
static xRTOS_Event_Context_t s_evt;
static xRTOS_Queue_Context_t s_queue;
static uint8_t s_queue_storage[8U * sizeof(uint32_t)];

// -- Per-phase result variables ---------------------------------------------

static volatile uint32_t s_result;      // general pass counter / value
static volatile bool s_rr_stop;         // phase 8 stop flag
static volatile uint32_t s_rr_count[3]; // phase 8 per-spinner counters

// -- assert hook -----------------------------------------------------------

void xassert_system_halt(void)
{
    while (1)
    {
    }
}

// -- Helper: create a helper task and wait for it to exit ------------------

static void create_helper(xRTOS_Task_Context_t *ctx,
                          uint32_t task_id,
                          uint32_t priority,
                          xRTOS_Task_Entry_t entry,
                          void *arg,
                          uint32_t *stack,
                          uint32_t stack_words)
{
    (void)xRTOS_Task_Create(
        ctx,
        &(xRTOS_Task_Config_t){
            .task_id = task_id, .priority = priority, .entry = entry, .entry_arg = arg, .stack_mem = stack, .stack_words = stack_words});
}

// ===========================================================================
// Phase 1 - counting semaphore
// ===========================================================================

static void phase1_helper(void *arg)
{
    (void)arg;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        if (xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER) == xRETURN_xRTOS_OK)
        {
            s_result++;
        }
    }
    xRTOS_Task_Exit();
}

static bool run_phase1(void)
{
    s_result = 0U;
    (void)xRTOS_Sem_Init(&s_sem, 5U, 5U, "CountSem"); // pre-loaded counting sem

    create_helper(&s_helper1_ctx, 1U, 2U, phase1_helper, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(20U); // helper drains 5 tokens and exits

    return (s_result == 5U);
}

// ===========================================================================
// Phase 2 - semaphore timeout
// ===========================================================================

static void phase2_helper(void *arg)
{
    (void)arg;
    xRETURN_t ret = xRTOS_Sem_Take(&s_sem, 3U); // 3-tick timeout on empty sem
    if (ret == xRETURN_xERR_xRTOS_TIMEOUT)
    {
        s_result = 1U;
    }
    xRTOS_Task_Exit();
}

static bool run_phase2(void)
{
    s_result = 0U;
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, "BinarySem"); // locked binary sem - never given

    create_helper(&s_helper1_ctx, 1U, 2U, phase2_helper, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(10U); // wait long enough for helper's 3-tick timeout

    return (s_result == 1U);
}

// ===========================================================================
// Phase 3 - mutex contention
// ===========================================================================

static void phase3_h1(void *arg)
{
    (void)arg;
    (void)xRTOS_Mutex_Lock(&s_mtx, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Task_Delay(5U); // hold mutex for 5 ticks while h2 contends
    (void)xRTOS_Mutex_Unlock(&s_mtx);
    xRTOS_Task_Exit();
}

static void phase3_h2(void *arg)
{
    (void)arg;
    // h1 (prio 2) holds the mutex; h2 (prio 3) must block until h1 unlocks
    xRETURN_t ret = xRTOS_Mutex_Lock(&s_mtx, 20U);
    if (ret == xRETURN_xRTOS_OK)
    {
        s_result++;
        (void)xRTOS_Mutex_Unlock(&s_mtx);
    }
    xRTOS_Task_Exit();
}

static bool run_phase3(void)
{
    s_result = 0U;
    (void)xRTOS_Mutex_Init(&s_mtx, "Mtx");

    // h1 has higher priority; it will acquire the mutex first.
    create_helper(&s_helper1_ctx, 1U, 2U, phase3_h1, NULL, s_helper1_stack, 256U);
    create_helper(&s_helper2_ctx, 2U, 3U, phase3_h2, NULL, s_helper2_stack, 256U);

    (void)xRTOS_Task_Delay(30U);

    return (s_result == 1U);
}

// ===========================================================================
// Phase 4 - event flags WAIT_ANY
// ===========================================================================

static volatile uint32_t s_event_matched;

static void phase4_helper(void *arg)
{
    (void)arg;
    uint32_t matched = 0U;
    xRETURN_t ret = xRTOS_Event_Wait(&s_evt, 0x3U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT, xRTOS_WAIT_FOREVER, &matched);
    if (ret == xRETURN_xRTOS_OK)
    {
        s_event_matched = matched;
        s_result = 1U;
    }
    xRTOS_Task_Exit();
}

static bool run_phase4(void)
{
    s_result = 0U;
    s_event_matched = 0U;
    (void)xRTOS_Event_Init(&s_evt, "Evt");

    create_helper(&s_helper1_ctx, 1U, 2U, phase4_helper, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(3U); // let helper block in Event_Wait

    (void)xRTOS_Event_Set(&s_evt, 0x1U); // ANY condition: 0x1 matches mask 0x3

    (void)xRTOS_Task_Delay(5U);

    return (s_result == 1U) && (s_event_matched == 0x1U);
}

// ===========================================================================
// Phase 5 - event flags WAIT_ALL
// ===========================================================================

static void phase5_helper(void *arg)
{
    (void)arg;
    uint32_t matched = 0U;
    xRETURN_t ret = xRTOS_Event_Wait(&s_evt, 0x3U, xRTOS_EVENT_WAIT_ALL | xRTOS_EVENT_CLEAR_ON_EXIT, xRTOS_WAIT_FOREVER, &matched);
    if (ret == xRETURN_xRTOS_OK)
    {
        s_event_matched = matched;
        s_result = 1U;
    }
    xRTOS_Task_Exit();
}

static bool run_phase5(void)
{
    s_result = 0U;
    s_event_matched = 0U;
    (void)xRTOS_Event_Init(&s_evt, "Evt");

    create_helper(&s_helper1_ctx, 1U, 2U, phase5_helper, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(3U); // let helper block

    (void)xRTOS_Event_Set(&s_evt, 0x1U); // partial - helper stays blocked (ALL needs 0x3)

    (void)xRTOS_Task_Delay(3U);

    if (s_result != 0U)
    {
        return false; // should not have woken yet
    }

    (void)xRTOS_Event_Set(&s_evt, 0x2U); // completes the ALL condition

    (void)xRTOS_Task_Delay(5U);

    return (s_result == 1U) && (s_event_matched == 0x3U);
}

// ===========================================================================
// Phase 6 - queue FIFO ordering
// ===========================================================================

static void phase6_sender(void *arg)
{
    (void)arg;
    for (uint32_t i = 1U; i <= 5U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &i, xRTOS_WAIT_FOREVER);
    }
    xRTOS_Task_Exit();
}

static void phase6_receiver(void *arg)
{
    (void)arg;
    uint32_t passes = 0U;
    for (uint32_t expected = 1U; expected <= 5U; expected++)
    {
        uint32_t item = 0U;
        xRETURN_t ret = xRTOS_Queue_Receive(&s_queue, &item, xRTOS_WAIT_FOREVER);
        if ((ret == xRETURN_xRTOS_OK) && (item == expected))
        {
            passes++;
        }
    }
    s_result = passes;
    xRTOS_Task_Exit();
}

static bool run_phase6(void)
{
    s_result = 0U;
    (void)xRTOS_Queue_Init(&s_queue, s_queue_storage, sizeof(uint32_t), 8U, "Queue");

    // h2 (prio 2, higher) receives; h1 (prio 3, lower) sends.
    // h2 blocks on empty queue; h1 sends one item at a time, h2 wakes each time.
    create_helper(&s_helper2_ctx, 2U, 2U, phase6_receiver, NULL, s_helper2_stack, 256U);
    create_helper(&s_helper1_ctx, 1U, 3U, phase6_sender, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(30U);

    return (s_result == 5U);
}

// ===========================================================================
// Phase 7 - task notification
// ===========================================================================

static void phase7_helper(void *arg)
{
    (void)arg;
    uint32_t value = 0U;
    xRETURN_t ret = xRTOS_Task_Notify_Wait(0U, 0xFFFFFFFFU, &value, xRTOS_WAIT_FOREVER);
    if (ret == xRETURN_xRTOS_OK)
    {
        s_result = value;
    }
    xRTOS_Task_Exit();
}

static bool run_phase7(void)
{
    s_result = 0U;

    create_helper(&s_helper1_ctx, 1U, 2U, phase7_helper, NULL, s_helper1_stack, 256U);

    (void)xRTOS_Task_Delay(3U); // let helper block in Notify_Wait

    (void)xRTOS_Task_Notify(1U, 0xBEEFU);

    (void)xRTOS_Task_Delay(5U);

    return (s_result == 0xBEEFU);
}

// ===========================================================================
// Phase 8 - round-robin fairness with 3 equal-priority spinners
// ===========================================================================

static void phase8_spinner(void *arg)
{
    volatile uint32_t *count = (volatile uint32_t *)arg;
    while (!s_rr_stop)
    {
        (*count)++;
    }
    xRTOS_Task_Exit();
}

static bool run_phase8(void)
{
    s_rr_stop = false;
    s_rr_count[0] = 0U;
    s_rr_count[1] = 0U;
    s_rr_count[2] = 0U;

    create_helper(&s_helper1_ctx, 1U, 2U, phase8_spinner, (void *)&s_rr_count[0], s_helper1_stack, 256U);
    create_helper(&s_helper2_ctx, 2U, 2U, phase8_spinner, (void *)&s_rr_count[1], s_helper2_stack, 256U);
    create_helper(&s_helper3_ctx, 3U, 2U, phase8_spinner, (void *)&s_rr_count[2], s_helper3_stack, 256U);

    (void)xRTOS_Task_Delay(40U);

    s_rr_stop = true;

    (void)xRTOS_Task_Delay(8U); // allow all spinners to see the flag and exit

    uint32_t min_c = s_rr_count[0];
    uint32_t max_c = s_rr_count[0];
    for (uint32_t i = 1U; i < 3U; i++)
    {
        if (s_rr_count[i] < min_c)
        {
            min_c = s_rr_count[i];
        }
        if (s_rr_count[i] > max_c)
        {
            max_c = s_rr_count[i];
        }
    }

    uart_puts("  rr: min=");
    uart_puti(min_c);
    uart_puts(" max=");
    uart_puti(max_c);
    uart_puts("\n");

    return (min_c > 0U) && (max_c < (min_c * 2U));
}

// ===========================================================================
// Supervisor
// ===========================================================================

static void supervisor_entry(void *arg)
{
    (void)arg;

    uart_puts("\nxRTOS Regression Test\n");

    typedef bool (*phase_fn_t)(void);
    static const phase_fn_t phases[8] = {
        run_phase1, run_phase2, run_phase3, run_phase4, run_phase5, run_phase6, run_phase7, run_phase8,
    };

    static const char *const names[8] = {
        "sem-counting", "sem-timeout", "mutex", "event-any", "event-all", "queue-fifo", "notify", "round-robin",
    };

    uint32_t pass_count = 0U;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        bool ok = phases[i]();
        uart_puts("  Phase ");
        uart_puti(i + 1U);
        uart_puts(" (");
        uart_puts(names[i]);
        uart_puts("): ");
        uart_puts(ok ? "PASS" : "FAIL");
        uart_puts("\n");
        if (ok)
        {
            pass_count++;
        }
    }

    uart_puts("\n=======================================================\n");
    if (pass_count == 8U)
    {
        uart_puts("REGRESSION PASS\n");
    }
    else
    {
        uart_puts("REGRESSION FAIL: ");
        uart_puti(pass_count);
        uart_puts("/8 phases passed\n");
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
}

void xrtos_port_arm_r5_irq_handler(void)
{
    uint32_t iar = GICC_IAR;
    uint32_t interrupt_id = iar & 0x3FFU;

    if (interrupt_id == 36U)
    {
        Timer1IntClr = 1U;
        bool should_yield = false;
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    GICC_EOIR = iar;
}

#endif // BOARD_REALVIEW_PB_A8

// -- main ------------------------------------------------------------------

int main(void)
{
    uart_init();
    uart_puts("\nxRTOS Regression QEMU Test\n");

    xRETURN_t ret = xRTOS_Kernel_Init(&s_kernel, &xrtos_arm_r5_port_ops);
    if (ret != xRETURN_xRTOS_OK)
    {
        uart_puts("Kernel init FAILED\n");
        return 1;
    }

    (void)xRTOS_Task_Create(&s_idle_ctx, &(xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                                                .priority = xRTOS_IDLE_PRIORITY,
                                                                .entry = idle_entry,
                                                                .entry_arg = NULL,
                                                                .stack_mem = s_idle_stack,
                                                                .stack_words = 128U});

    (void)xRTOS_Task_Create(&s_supervisor_ctx, &(xRTOS_Task_Config_t){.task_id = 0U,
                                                                      .priority = 0U,
                                                                      .entry = supervisor_entry,
                                                                      .entry_arg = NULL,
                                                                      .stack_mem = s_supervisor_stack,
                                                                      .stack_words = 384U});

    hardware_init();

    uart_puts("Starting scheduler...\n");
    xRTOS_Kernel_Start();

    return 0;
}

// EOF /////////////////////////////////////////////////////////////////////////////
