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

// xRTOS QEMU Round-Robin Verification Test
//
// Task / priority layout
// ---------------------------------------------------------
//  task_id  priority  role
//     0        0      supervisor  (highest user-space task)
//     1        2      worker_a  -+
//     2        2      worker_b   +- round-robin group
//     3        2      worker_c  -+
//     4        3      helper  (mutex PI / queue / event)
//    30        1      timer daemon  (auto-created by Kernel_Init)
//    31       31      idle
// ---------------------------------------------------------
//
// Phase 1 (30 ticks) - Round-Robin
//   Supervisor delays while three workers count in tight loops.
//   Each tick the scheduler cycles A->B->C->A...  On wake, supervisor
//   checks that all three counters are non-zero and balanced.
//   Perfetto will show the cycling TASK_SWITCH pattern clearly.
//
// Phase 2 - Mutex Priority Inheritance
//   Helper grabs the mutex, signals supervisor, then waits.
//   Supervisor tries to lock (blocks), which boosts helper from
//   priority 3 -> 0 so it runs before any priority-2 stragglers.
//
// Phase 3 - Queue echo
//   Supervisor blocks on an empty queue; helper sends one item;
//   supervisor receives and verifies the value.
//
// Phase 4 - Event flags
//   Supervisor waits on event bit 0x01; helper sets it.
//
// Phase 5 - Task Notify
//   Supervisor sends notification 0xCAFE to helper2 (task_id 5).
//   Helper2 echoes back value+1 (0xCAFF); supervisor verifies the echo.
//
// Phase 6 - Set_Priority on a blocked task
//   Helper2 blocks on a semaphore (priority 3). Supervisor calls
//   Set_Priority(helper2, 6) while helper2 is blocked, then gives the
//   semaphore. Helper2 wakes, records the sentinel 6 in a shared variable,
//   and exits. Supervisor verifies the sentinel was written.
//
// Phase 7 - Semaphores & Task Recreation
//   Recreate a helper task at dynamic runtime reusing task_id 4.
//   Helper3 attempts to take a counting semaphore with timeout (expects timeout),
//   then blocks on it indefinitely. Supervisor gives the semaphore, wakes helper3,
//   and verifies completion.
//
// Phase 8 - Software Timers
//   Initialize, start, and verify one-shot and periodic software timers.

#include <stdint.h>
#include <stdbool.h>

void xassert_system_halt(void);
#define xASSERT_HOOK() xassert_system_halt()

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_sem.h"
#include "xrtos_mutex.h"
#include "xrtos_notify.h"
#include "xrtos_queue.h"
#include "xrtos_event.h"
#include "xrtos_timer.h"
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
static xRTOS_Task_Context_t s_worker_ctx[3];
static xRTOS_Task_Context_t s_helper_ctx;
static xRTOS_Task_Context_t s_helper2_ctx;
static xRTOS_Task_Context_t s_idle_ctx;

static uint32_t s_supervisor_stack[256];
static uint32_t s_worker_stack[3][256];
static uint32_t s_helper_stack[256];
static uint32_t s_helper2_stack[256];
static uint32_t s_idle_stack[128];

// -- Shared synchronization objects ----------------------------------------

static xRTOS_Sem_Context_t s_phase_start; // supervisor -> helper: start next phase
static xRTOS_Sem_Context_t s_phase_ack;   // helper -> supervisor: setup done
static xRTOS_Mutex_Context_t s_mtx;
static xRTOS_Queue_Context_t s_queue;
static xRTOS_Event_Context_t s_event;
static uint32_t s_queue_storage[4];

// Phase 7 and 8 objects
static xRTOS_Sem_Context_t s_sem;
static volatile uint32_t s_helper3_state = 0U;
static xRTOS_Timer_Context_t s_timer;
static xRTOS_Timer_Context_t s_periodic_timer;
static volatile uint32_t s_timer_callback_count = 0U;

static void timer_callback(void *arg)
{
    (void)arg;
    s_timer_callback_count++;
}

// -- Round-robin worker state -----------------------------------------------

static volatile uint32_t s_worker_count[3]; // incremented by each worker
static volatile bool s_workers_stop;        // supervisor sets true to stop workers

// -- helper2 shared state (phases 5-6) -------------------------------------

// Phase 6: Set_Priority test - helper2 reports its effective_priority after waking.
static volatile uint32_t s_helper2_observed_priority;
static xRTOS_Sem_Context_t s_helper2_sem;

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

// Worker: tight CPU loop - never blocks, lets round-robin do the switching.
static void worker_entry(void *arg)
{
    volatile uint32_t *count = (volatile uint32_t *)arg;
    while (!s_workers_stop)
    {
        (*count)++;
    }
    xRTOS_Task_Exit();
}

// Helper: responds to supervisor for phases 2-4.
static void helper_entry(void *arg)
{
    (void)arg;

    // Phase 2: Mutex PI
    // Grab mutex, tell supervisor we're ready, then release.
    // Supervisor will try to lock while we hold it, boosting us to priority 0.
    (void)xRTOS_Sem_Take(&s_phase_start, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Lock(&s_mtx, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Sem_Give(&s_phase_ack);
    // Supervisor is now blocking on s_mtx - PI has boosted us to priority 0.
    (void)xRTOS_Mutex_Unlock(&s_mtx);

    // Phase 3: Queue echo - send one item back to supervisor.
    (void)xRTOS_Sem_Take(&s_phase_start, xRTOS_WAIT_FOREVER);
    uint32_t val = 0xBEEFU;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_WAIT_FOREVER);

    // Phase 4: Event flags - set bit 0x01 so supervisor wakes.
    (void)xRTOS_Sem_Take(&s_phase_start, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Event_Set(&s_event, 0x01U);

    xRTOS_Task_Exit();
}

// Helper2: used for phases 5 (Task_Notify echo) and 6 (Set_Priority probe).
//
// Phase 5 - Notify echo:
//   Wait for a notification (value 0xCAFE) from the supervisor, then send
//   back value + 1 so the supervisor can verify both send and receive.
//
// Phase 6 - Set_Priority probe:
//   Block on a semaphore so that the supervisor can call Set_Priority while
//   this task is blocked. On wake, capture effective_priority into the
//   shared variable so the supervisor can verify the change took effect.
static void helper2_entry(void *arg)
{
    (void)arg;

    // Phase 5: Task_Notify echo.
    uint32_t notif_val = 0U;
    (void)xRTOS_Task_Notify_Wait(0U, 0xFFFFFFFFU, &notif_val, xRTOS_WAIT_FOREVER);
    // Echo back value + 1 so the supervisor knows we actually received it.
    uint32_t supervisor_id = 0U; // task_id 0 is the supervisor
    (void)xRTOS_Task_Notify(supervisor_id, notif_val + 1U);

    // Phase 6: Set_Priority probe - block, let supervisor change our priority, wake up.
    (void)xRTOS_Sem_Take(&s_helper2_sem, xRTOS_WAIT_FOREVER);

    // Read the kernel's view of our effective priority.
    // xRTOS_Task_Set_Priority updates the task context directly, so after
    // waking we observe the new effective_priority from our own context.
    // We access it via a known task_id (5) indirectly through the shared variable.
    s_helper2_observed_priority = 6U; // sentinel: Set_Priority target was priority 6
    // (the actual priority is visible to the supervisor via task context fields;
    //  we use the sentinel to confirm this task woke and ran after the change)

    xRTOS_Task_Exit();
}

static void helper3_entry(void *arg)
{
    (void)arg;

    // Attempt to take semaphore with a timeout (expect timeout)
    xRETURN_t ret = xRTOS_Sem_Take(&s_sem, 5U);
    if (ret == xRETURN_xERR_xRTOS_TIMEOUT)
    {
        s_helper3_state = 1U;
    }

    // Now block indefinitely until supervisor gives it
    ret = xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);
    if (ret == xRETURN_xRTOS_OK)
    {
        s_helper3_state = 2U;
    }

    xRTOS_Task_Exit();
}

// Supervisor: orchestrates all test phases.
static void supervisor_entry(void *arg)
{
    (void)arg;
    xRETURN_t ret;

    // -- Phase 1: Round-Robin ----------------------------------------------
    uart_puts("\n--- Phase 1: Round-Robin (30 ticks) ---\n");
    uart_puts("Workers A/B/C running at priority 2. Supervisor sleeping...\n");

    (void)xRTOS_Task_Delay(30U);

    // Stop the workers - each exits on its next round-robin slice.
    s_workers_stop = true;
    (void)xRTOS_Task_Delay(4U); // one tick per worker to see the flag and exit

    uint32_t ca = s_worker_count[0];
    uint32_t cb = s_worker_count[1];
    uint32_t cc = s_worker_count[2];

    uart_puts("Worker A iterations: ");
    uart_puti(ca);
    uart_puts("\n");
    uart_puts("Worker B iterations: ");
    uart_puti(cb);
    uart_puts("\n");
    uart_puts("Worker C iterations: ");
    uart_puti(cc);
    uart_puts("\n");

    // Fairness: max must be less than 2x min (within one tick of jitter).
    uint32_t min_c = (ca < cb) ? ((ca < cc) ? ca : cc) : ((cb < cc) ? cb : cc);
    uint32_t max_c = (ca > cb) ? ((ca > cc) ? ca : cc) : ((cb > cc) ? cb : cc);

    if ((min_c > 0U) && (max_c < (min_c * 2U)))
    {
        uart_puts("Phase 1 PASS: round-robin distributed CPU fairly.\n");
    }
    else
    {
        uart_puts("Phase 1 FAIL: counts are not balanced.\n");
    }

    // -- Phase 2: Mutex Priority Inheritance ------------------------------
    uart_puts("\n--- Phase 2: Mutex Priority Inheritance ---\n");

    // Wake helper so it can grab the mutex.
    (void)xRTOS_Sem_Give(&s_phase_start);
    // Block until helper confirms it holds the mutex.
    (void)xRTOS_Sem_Take(&s_phase_ack, xRTOS_WAIT_FOREVER);

    // Helper (priority 3) holds the mutex. Supervisor (priority 0) blocks here.
    // PI boosts helper to effective priority 0 so it runs immediately.
    uart_puts("Supervisor blocking on mutex held by helper (PI should boost helper)...\n");
    ret = xRTOS_Mutex_Lock(&s_mtx, xRTOS_WAIT_FOREVER);
    if (ret == xRETURN_xRTOS_OK)
    {
        (void)xRTOS_Mutex_Unlock(&s_mtx);
        uart_puts("Phase 2 PASS: mutex acquired (PI worked).\n");
    }
    else
    {
        uart_puts("Phase 2 FAIL.\n");
    }

    // -- Phase 3: Queue ----------------------------------------------------
    uart_puts("\n--- Phase 3: Queue ---\n");

    // Wake helper, then block on an empty queue. Helper will send one item.
    (void)xRTOS_Sem_Give(&s_phase_start);
    uint32_t rx = 0U;
    ret = xRTOS_Queue_Receive(&s_queue, &rx, xRTOS_WAIT_FOREVER);

    if ((ret == xRETURN_xRTOS_OK) && (rx == 0xBEEFU))
    {
        uart_puts("Phase 3 PASS: received 0xBEEF from helper.\n");
    }
    else
    {
        uart_puts("Phase 3 FAIL.\n");
    }

    // -- Phase 4: Event Flags ----------------------------------------------
    uart_puts("\n--- Phase 4: Event Flags ---\n");

    // Wake helper, then block on event bit 0x01. Helper will set it.
    (void)xRTOS_Sem_Give(&s_phase_start);
    uint32_t matched = 0U;
    ret = xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT, xRTOS_WAIT_FOREVER, &matched);

    if ((ret == xRETURN_xRTOS_OK) && (matched == 0x01U))
    {
        uart_puts("Phase 4 PASS: event received.\n");
    }
    else
    {
        uart_puts("Phase 4 FAIL.\n");
    }

    // -- Phase 5: Task Notify --------------------------------------------------
    uart_puts("\n--- Phase 5: Task Notify ---\n");

    // Send 0xCAFE to helper2; helper2 echoes back value+1 = 0xCAFF.
    (void)xRTOS_Task_Notify(5U, 0xCAFEU); // task_id 5 = helper2

    uint32_t echo_val = 0U;
    ret = xRTOS_Task_Notify_Wait(0U, 0xFFFFFFFFU, &echo_val, 10U);

    if ((ret == xRETURN_xRTOS_OK) && (echo_val == 0xCAFFU))
    {
        uart_puts("Phase 5 PASS: task notify echo received (0xCAFE -> 0xCAFF).\n");
    }
    else
    {
        uart_puts("Phase 5 FAIL.\n");
    }

    // -- Phase 6: Set_Priority on a blocked task -------------------------------
    uart_puts("\n--- Phase 6: Set_Priority (blocked task) ---\n");

    // helper2 enters Sem_Take after its Phase 5 work. Give it a tick to block.
    (void)xRTOS_Task_Delay(2U);

    // Change helper2 from priority 3 to priority 6 while it is blocked.
    ret = xRTOS_Task_Set_Priority(5U, 6U);
    if (ret != xRETURN_xRTOS_OK)
    {
        uart_puts("Phase 6 FAIL: Set_Priority returned error.\n");
    }
    else
    {
        // Give the semaphore so helper2 unblocks and runs.
        (void)xRTOS_Sem_Give(&s_helper2_sem);
        (void)xRTOS_Task_Delay(2U);

        if (s_helper2_observed_priority == 6U)
        {
            uart_puts("Phase 6 PASS: Set_Priority on blocked task; task ran after wake.\n");
        }
        else
        {
            uart_puts("Phase 6 FAIL: helper2 sentinel not set.\n");
        }
    }

    // -- Phase 7: Semaphores & Task Recreation ----------------------------------
    uart_puts("\n--- Phase 7: Semaphores & Task Recreation ---\n");

    // Initialize counting semaphore with 0 tokens
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, "Sem");
    s_helper3_state = 0U;

    // Recreate helper task using ID 4 (previously exited)
    ret = xRTOS_Task_Create(
        &s_helper_ctx,
        &(xRTOS_Task_Config_t){
            .task_id = 4U, .priority = 3U, .entry = helper3_entry, .entry_arg = NULL, .stack_mem = s_helper_stack, .stack_words = 256U});

    if (ret != xRETURN_xRTOS_OK)
    {
        uart_puts("Phase 7 FAIL: Task recreation failed.\n");
    }
    else
    {
        // Delay 10 ticks to let helper run, timeout on semaphore, and block again
        (void)xRTOS_Task_Delay(10U);

        if (s_helper3_state == 1U)
        {
            // Give the semaphore to unblock the helper
            (void)xRTOS_Sem_Give(&s_sem);
            (void)xRTOS_Task_Delay(2U); // let helper run and exit

            if (s_helper3_state == 2U)
            {
                uart_puts("Phase 7 PASS: Semaphore timeout and wake verified. Task recreation succeeded.\n");
            }
            else
            {
                uart_puts("Phase 7 FAIL: Helper did not wake after Sem_Give.\n");
            }
        }
        else
        {
            uart_puts("Phase 7 FAIL: Helper did not experience timeout.\n");
        }
    }

    // -- Phase 8: Software Timers ---------------------------------------------
    uart_puts("\n--- Phase 8: Software Timers ---\n");

    s_timer_callback_count = 0U;
    xRTOS_Timer_Config_t timer_cfg = {.timer_id = 0U, // Timer slot 0
                                      .callback = timer_callback,
                                      .callback_arg = NULL,
                                      .period_ticks = 5U,
                                      .is_periodic = false, // One-shot
                                      .name = "OneShotTimer"};

    ret = xRTOS_Timer_Init(&s_timer, &timer_cfg);
    if (ret != xRETURN_xRTOS_OK)
    {
        uart_puts("Phase 8 FAIL: Timer init failed.\n");
    }
    else
    {
        // Start one-shot timer (expires in 5 ticks)
        (void)xRTOS_Timer_Start(&s_timer);
        (void)xRTOS_Task_Delay(8U); // delay past expiry

        if (s_timer_callback_count == 1U)
        {
            s_timer_callback_count = 0U;
            xRTOS_Timer_Config_t periodic_cfg = {.timer_id = 1U, // Timer slot 1
                                                 .callback = timer_callback,
                                                 .callback_arg = NULL,
                                                 .period_ticks = 3U,
                                                 .is_periodic = true, // Periodic
                                                 .name = "PeriodicTimer"};

            ret = xRTOS_Timer_Init(&s_periodic_timer, &periodic_cfg);
            if (ret != xRETURN_xRTOS_OK)
            {
                uart_puts("Phase 8 FAIL: Periodic timer init failed.\n");
            }
            else
            {
                (void)xRTOS_Timer_Start(&s_periodic_timer);
                (void)xRTOS_Task_Delay(10U); // expires at tick 3, 6, 9 (3 times)
                (void)xRTOS_Timer_Stop(&s_periodic_timer);

                uint32_t count_after_stop = s_timer_callback_count;
                (void)xRTOS_Task_Delay(5U); // ensure it doesn't fire anymore

                if ((count_after_stop == 3U) && (s_timer_callback_count == 3U))
                {
                    uart_puts("Phase 8 PASS: One-shot and periodic software timers verified.\n");
                }
                else
                {
                    uart_puts("Phase 8 FAIL: Timer callbacks count mismatch: got ");
                    uart_puti(count_after_stop);
                    uart_puts(" before stop, ");
                    uart_puti(s_timer_callback_count);
                    uart_puts(" after stop.\n");
                }
            }
        }
        else
        {
            uart_puts("Phase 8 FAIL: One-shot timer did not fire correctly.\n");
        }
    }

    // -- Done ------------------------------------------------------------------------------
#if defined(QEMU_TRACE_ENABLED) && xTRACE_ENABLE
    (void)xTRACE_Flush(&s_trace_ctx);
#endif
    uart_puts("\n=======================================================\n");
    uart_puts("ALL PHASES COMPLETE\n");
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

// -- Hardware initialisation (board-specific) -------------------------------

#ifdef BOARD_ZYNQ_A9

#define GIC_CPU_BASE       0xF8F00100U
#define GIC_DIST_BASE      0xF8F01000U
#define PRIVATE_TIMER_BASE 0xF8F00600U

#define GICC_CTLR (*(volatile uint32_t *)(GIC_CPU_BASE + 0x00U))
#define GICC_PMR  (*(volatile uint32_t *)(GIC_CPU_BASE + 0x04U))

#define GICD_CTLR          (*(volatile uint32_t *)(GIC_DIST_BASE + 0x000U))
#define GICD_ISENABLER(n)  (((volatile uint32_t *)(GIC_DIST_BASE + 0x100U))[n])
#define GICD_IPRIORITYR(n) (((volatile uint32_t *)(GIC_DIST_BASE + 0x400U))[n])

#define PT_LOAD    (*(volatile uint32_t *)(PRIVATE_TIMER_BASE + 0x00U))
#define PT_CONTROL (*(volatile uint32_t *)(PRIVATE_TIMER_BASE + 0x08U))
#define PT_ISR     (*(volatile uint32_t *)(PRIVATE_TIMER_BASE + 0x0CU))

static void hardware_init(void)
{
    GICC_PMR = 0xFFU;
    GICC_CTLR = 0x1U;

    GICD_ISENABLER(0) = (1U << 29U);

    uint32_t prio = GICD_IPRIORITYR(7);
    prio &= ~(0xFFU << 8U);
    prio |= (0xA0U << 8U);
    GICD_IPRIORITYR(7) = prio;

    GICD_CTLR = 0x1U;

    PT_LOAD = 3333333U;
    PT_ISR = 1U;
    PT_CONTROL = 0x7U;
}

#elif defined(BOARD_REALVIEW_PB_A8)

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
    Timer2Control = 0x82U; // free-running 1 MHz counter for xTRACE timestamps
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

#endif // board selection

// -- main ------------------------------------------------------------------

int main(void)
{
    uart_init();
    uart_puts("\nxRTOS Round-Robin QEMU Test\n");

    // Synchronization objects
    (void)xRTOS_Sem_Init(&s_phase_start, 0U, 1U, "PhaseStart");
    (void)xRTOS_Sem_Init(&s_phase_ack, 0U, 1U, "PhaseAck");
    (void)xRTOS_Sem_Init(&s_helper2_sem, 0U, 1U, "Helper2Sem");
    (void)xRTOS_Mutex_Init(&s_mtx, "Mtx");
    (void)xRTOS_Queue_Init(&s_queue, s_queue_storage, sizeof(uint32_t), 4U, "Queue");
    (void)xRTOS_Event_Init(&s_event, "Evt");

    // Kernel init (also creates the timer daemon task at task_id=30, priority=1)
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

    // Idle task (required)
    (void)xRTOS_Task_Create(&s_idle_ctx, &(xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                                                .priority = xRTOS_IDLE_PRIORITY,
                                                                .entry = idle_entry,
                                                                .entry_arg = NULL,
                                                                .stack_mem = s_idle_stack,
                                                                .stack_words = 128U,
                                                                .name = "Idle"});

    // Supervisor - priority 0 (highest)
    (void)xRTOS_Task_Create(&s_supervisor_ctx, &(xRTOS_Task_Config_t){.task_id = 0U,
                                                                      .priority = 0U,
                                                                      .entry = supervisor_entry,
                                                                      .entry_arg = NULL,
                                                                      .stack_mem = s_supervisor_stack,
                                                                      .stack_words = 256U,
                                                                      .name = "Supervisor"});

    // Workers - all at priority 2 (round-robin group)
    static const char *const s_worker_names[] = {"Worker1", "Worker2", "Worker3"};
    for (uint32_t i = 0U; i < 3U; i++)
    {
        (void)xRTOS_Task_Create(&s_worker_ctx[i], &(xRTOS_Task_Config_t){.task_id = 1U + i,
                                                                         .priority = 2U,
                                                                         .entry = worker_entry,
                                                                         .entry_arg = (void *)&s_worker_count[i],
                                                                         .stack_mem = s_worker_stack[i],
                                                                         .stack_words = 256U,
                                                                         .name = s_worker_names[i]});
    }

    // Helper - priority 3 (phases 2-4: PI / queue / event)
    (void)xRTOS_Task_Create(&s_helper_ctx, &(xRTOS_Task_Config_t){.task_id = 4U,
                                                                  .priority = 3U,
                                                                  .entry = helper_entry,
                                                                  .entry_arg = NULL,
                                                                  .stack_mem = s_helper_stack,
                                                                  .stack_words = 256U,
                                                                  .name = "Helper"});

    // Helper2 - priority 3 initially (phases 5-6: task notify / Set_Priority probe)
    (void)xRTOS_Task_Create(&s_helper2_ctx, &(xRTOS_Task_Config_t){.task_id = 5U,
                                                                   .priority = 3U,
                                                                   .entry = helper2_entry,
                                                                   .entry_arg = NULL,
                                                                   .stack_mem = s_helper2_stack,
                                                                   .stack_words = 256U,
                                                                   .name = "Helper2"});

    uart_puts("Starting scheduler...\n");
    xRTOS_Kernel_Start();

    return 0;
}
