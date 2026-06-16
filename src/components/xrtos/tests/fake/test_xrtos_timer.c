// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file test_xrtos_timer.c
// @brief Host tests for xRTOS software timers (Phase 16).
//
// Tests cover:
//   1. xRTOS_Timer_Init   - field initialization and argument validation.
//   2. xRTOS_Timer_Start  - is_active, active_timers_map bit, expiry_tick.
//   3. xRTOS_Timer_Stop   - clears is_active and active_timers_map bit.
//   4. Tick integration   - one-shot fires exactly once and self-deactivates;
//      periodic reloads and fires on every period; Stop prevents firing.
//
// Host simulation note:
//   xRTOS_Tick_Increment_From_ISR is called directly in tests to drive the
//   tick counter.  Timer callbacks run synchronously in the Tick call.
//   No task registration is required for timer-only tests; the task timeout
//   loop finds nothing set in the empty timeout_map and completes in O(32).
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_return.h"
#include "xrtos_tick.h"
#include "xrtos_timer.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Timer_Context_t s_timer;
static xRTOS_Timer_Context_t s_timer_b;

static uint32_t s_callback_count;
static uint32_t s_disable_count;
static uint32_t s_enable_count;
static void *s_callback_arg_received;

// HELPERS /////////////////////////////////////////////////////////////////////

static void counting_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg)
{
    (void)task_ctx;
    (void)entry;
    (void)arg;
}

static void counting_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    (void)task_ctx;
}

static void counting_yield(void)
{
}

static uint32_t counting_disable_interrupts(void)
{
    s_disable_count++;
    return 0U;
}

static void counting_enable_interrupts(uint32_t saved_state)
{
    (void)saved_state;
    s_enable_count++;
}

static bool counting_is_in_isr(void)
{
    return false;
}

static const xRTOS_Port_Ops_t s_counting_port_ops = {
    .init_task_stack = counting_init_task_stack,
    .start_first_task = counting_start_first_task,
    .yield = counting_yield,
    .disable_interrupts = counting_disable_interrupts,
    .enable_interrupts = counting_enable_interrupts,
    .is_in_isr = counting_is_in_isr,
};

static void counting_callback(void *arg)
{
    s_callback_count++;
    s_callback_arg_received = arg;
}

static void tick_n(uint32_t n)
{
    bool yield;
    for (uint32_t i = 0U; i < n; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }
}

static void init_timer(uint32_t timer_id, uint32_t period, bool is_periodic)
{
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = timer_id;
    cfg.callback = counting_callback;
    cfg.callback_arg = NULL;
    cfg.period_ticks = period;
    cfg.is_periodic = is_periodic;
    cfg.name = NULL;
    (void)xRTOS_Timer_Init(&s_timer, &cfg);
}

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    s_callback_count = 0U;
    s_disable_count = 0U;
    s_enable_count = 0U;
    s_callback_arg_received = (void *)0xDEADBEEFU;
}

void tearDown(void)
{
}

// INIT TESTS //////////////////////////////////////////////////////////////////

void test_timer_init_null_ctx(void)
{
    xRTOS_Timer_Config_t cfg = {0U};
    cfg.callback = counting_callback;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Timer_Init(NULL, &cfg));
}

void test_timer_init_null_config(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Timer_Init(&s_timer, NULL));
}

void test_timer_init_null_callback(void)
{
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 0U;
    cfg.callback = NULL;
    cfg.callback_arg = NULL;
    cfg.period_ticks = 10U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Timer_Init(&s_timer, &cfg));
}

void test_timer_init_timer_id_out_of_range(void)
{
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = xRTOS_MAX_TIMERS;
    cfg.callback = counting_callback;
    cfg.callback_arg = NULL;
    cfg.period_ticks = 10U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Timer_Init(&s_timer, &cfg));
}

void test_timer_init_zero_period_returns_invalid_argument(void)
{
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 0U;
    cfg.callback = counting_callback;
    cfg.callback_arg = NULL;
    cfg.period_ticks = 0U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Timer_Init(&s_timer, &cfg));
}

void test_timer_init_slot_occupied_returns_invalid_argument(void)
{
    init_timer(0U, 10U, false); // occupies slot 0

    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 0U;
    cfg.callback = counting_callback;
    cfg.callback_arg = NULL;
    cfg.period_ticks = 5U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Timer_Init(&s_timer_b, &cfg));
}

void test_timer_init_sets_callback(void)
{
    init_timer(0U, 10U, false);
    TEST_ASSERT_EQUAL_PTR(counting_callback, s_timer.callback);
}

void test_timer_init_sets_period_ticks(void)
{
    init_timer(0U, 42U, false);
    TEST_ASSERT_EQUAL_UINT32(42U, s_timer.period_ticks);
}

void test_timer_init_sets_timer_id(void)
{
    init_timer(3U, 10U, false);
    TEST_ASSERT_EQUAL_UINT32(3U, s_timer.timer_id);
}

void test_timer_init_sets_is_periodic_true(void)
{
    init_timer(0U, 10U, true);
    TEST_ASSERT_TRUE(s_timer.is_periodic);
}

void test_timer_init_sets_is_periodic_false(void)
{
    init_timer(0U, 10U, false);
    TEST_ASSERT_FALSE(s_timer.is_periodic);
}

void test_timer_init_active_timers_map_bit_clear(void)
{
    init_timer(0U, 10U, false);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_init_registers_in_timer_table(void)
{
    init_timer(2U, 10U, false);
    TEST_ASSERT_EQUAL_PTR(&s_timer, s_kernel.timer_table[2U]);
}

void test_timer_init_masks_interrupts_while_registering(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);

    init_timer(2U, 10U, false);

    TEST_ASSERT_EQUAL_UINT32(1U, s_disable_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_enable_count);
}

void test_timer_init_passes_callback_arg(void)
{
    int sentinel = 42;
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 0U;
    cfg.callback = counting_callback;
    cfg.callback_arg = &sentinel;
    cfg.period_ticks = 1U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    (void)xRTOS_Timer_Init(&s_timer, &cfg);
    TEST_ASSERT_EQUAL_PTR(&sentinel, s_timer.callback_arg);
}

// START TESTS /////////////////////////////////////////////////////////////////

void test_timer_start_null_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Timer_Start(NULL));
}

void test_timer_start_unregistered_ctx_returns_invalid_argument(void)
{
    xRTOS_Timer_Context_t timer = {0U};
    timer.timer_id = 0U;
    timer.period_ticks = 1U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Timer_Start(&timer));
}

void test_timer_start_sets_active_timers_map_bit(void)
{
    init_timer(0U, 10U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_start_computes_expiry_tick(void)
{
    init_timer(0U, 7U, false);
    tick_n(3U); // tick_count = 3
    (void)xRTOS_Timer_Start(&s_timer);
    TEST_ASSERT_EQUAL_UINT32(10U, s_timer.expiry_tick); // 3 + 7
}

void test_timer_start_reset_deadline_when_already_active(void)
{
    init_timer(0U, 5U, false);
    tick_n(1U);
    (void)xRTOS_Timer_Start(&s_timer); // expiry = 1 + 5 = 6
    tick_n(2U);
    (void)xRTOS_Timer_Start(&s_timer); // restart: expiry = 3 + 5 = 8
    TEST_ASSERT_EQUAL_UINT32(8U, s_timer.expiry_tick);
}

void test_timer_start_masks_interrupts_while_arming(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    init_timer(0U, 5U, false);
    s_disable_count = 0U;
    s_enable_count = 0U;

    (void)xRTOS_Timer_Start(&s_timer);

    TEST_ASSERT_EQUAL_UINT32(1U, s_disable_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_enable_count);
}

// STOP TESTS //////////////////////////////////////////////////////////////////

void test_timer_stop_null_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Timer_Stop(NULL));
}

void test_timer_stop_unregistered_ctx_returns_invalid_argument(void)
{
    xRTOS_Timer_Context_t timer = {0U};
    timer.timer_id = 0U;
    timer.period_ticks = 1U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Timer_Stop(&timer));
}

void test_timer_stop_clears_active_timers_map_bit(void)
{
    init_timer(0U, 10U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    (void)xRTOS_Timer_Stop(&s_timer);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_stop_masks_interrupts_while_disarming(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    init_timer(0U, 5U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    s_disable_count = 0U;
    s_enable_count = 0U;

    (void)xRTOS_Timer_Stop(&s_timer);

    TEST_ASSERT_EQUAL_UINT32(1U, s_disable_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_enable_count);
}

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_ISR)
// TICK INTEGRATION - ONE-SHOT /////////////////////////////////////////////////

void test_timer_oneshot_callback_not_called_before_expiry(void)
{
    init_timer(0U, 5U, false);
    (void)xRTOS_Timer_Start(&s_timer); // expiry = tick 5

    tick_n(4U); // tick_count = 4; not yet expired

    TEST_ASSERT_EQUAL_UINT32(0U, s_callback_count);
}

void test_timer_oneshot_callback_called_at_expiry(void)
{
    init_timer(0U, 5U, false);
    (void)xRTOS_Timer_Start(&s_timer);

    tick_n(5U); // tick_count = 5 = expiry_tick

    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
}

void test_timer_oneshot_deactivated_after_fire(void)
{
    init_timer(0U, 3U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(3U);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_oneshot_callback_called_exactly_once(void)
{
    init_timer(0U, 3U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(10U); // well past expiry

    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
}

void test_timer_oneshot_callback_receives_arg(void)
{
    int sentinel = 99;
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 0U;
    cfg.callback = counting_callback;
    cfg.callback_arg = &sentinel;
    cfg.period_ticks = 1U;
    cfg.is_periodic = false;
    cfg.name = NULL;
    (void)xRTOS_Timer_Init(&s_timer, &cfg);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(1U);

    TEST_ASSERT_EQUAL_PTR(&sentinel, s_callback_arg_received);
}

// TICK INTEGRATION - PERIODIC /////////////////////////////////////////////////

void test_timer_periodic_fires_at_first_period(void)
{
    init_timer(0U, 4U, true);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(4U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
}

void test_timer_periodic_remains_active_after_fire(void)
{
    init_timer(0U, 4U, true);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(4U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_periodic_reloads_expiry(void)
{
    init_timer(0U, 4U, true);
    (void)xRTOS_Timer_Start(&s_timer); // expiry = 4
    tick_n(4U);                        // fires; expiry reloaded to 4 + 4 = 8

    TEST_ASSERT_EQUAL_UINT32(8U, s_timer.expiry_tick);
}

void test_timer_periodic_fires_multiple_times(void)
{
    init_timer(0U, 3U, true);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(9U); // fires at tick 3, 6, 9

    TEST_ASSERT_EQUAL_UINT32(3U, s_callback_count);
}

// TICK INTEGRATION - STOP PREVENTS CALLBACK ////////////////////////////////////

void test_timer_stop_before_expiry_prevents_callback(void)
{
    init_timer(0U, 10U, false);
    (void)xRTOS_Timer_Start(&s_timer);
    tick_n(5U);
    (void)xRTOS_Timer_Stop(&s_timer);
    tick_n(10U); // past where expiry would have been

    TEST_ASSERT_EQUAL_UINT32(0U, s_callback_count);
}

// MULTIPLE TIMERS /////////////////////////////////////////////////////////////

void test_two_timers_fire_independently(void)
{
    uint32_t count_b = 0U;

    // Timer A: slot 0, period 3, one-shot via s_timer.
    xRTOS_Timer_Config_t cfg_a;
    cfg_a.timer_id = 0U;
    cfg_a.callback = counting_callback;
    cfg_a.callback_arg = NULL;
    cfg_a.period_ticks = 3U;
    cfg_a.is_periodic = false;
    cfg_a.name = NULL;
    (void)xRTOS_Timer_Init(&s_timer, &cfg_a);

    // Timer B: slot 1, period 5, one-shot via s_timer_b with local counter.
    // Reuse counting_callback; distinguish by inspecting counts before/after.
    xRTOS_Timer_Config_t cfg_b;
    cfg_b.timer_id = 1U;
    cfg_b.callback = counting_callback;
    cfg_b.callback_arg = NULL;
    cfg_b.period_ticks = 5U;
    cfg_b.is_periodic = false;
    cfg_b.name = NULL;
    (void)xRTOS_Timer_Init(&s_timer_b, &cfg_b);

    (void)xRTOS_Timer_Start(&s_timer);
    (void)xRTOS_Timer_Start(&s_timer_b);

    tick_n(3U); // timer A fires; timer B hasn't yet (needs 5)
    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);

    uint32_t count_after_a = s_callback_count;
    tick_n(2U); // total 5 ticks; timer B fires
    count_b = s_callback_count - count_after_a;
    TEST_ASSERT_EQUAL_UINT32(1U, count_b);
}
#endif // xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_ISR

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
#include <setjmp.h>

static jmp_buf s_timer_exit_jmp;
static void test_timer_yield_jmp(void)
{
    longjmp(s_timer_exit_jmp, 1);
}

void test_timer_daemon_initializes_and_creates_task(void)
{
    TEST_ASSERT_NOT_NULL(s_kernel.task_table[xRTOS_TIMER_TASK_ID]);
    TEST_ASSERT_EQUAL_PTR(&s_kernel.timer_task_ctx, s_kernel.task_table[xRTOS_TIMER_TASK_ID]);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_CONFIG_TIMER_TASK_PRIORITY, s_kernel.timer_task_ctx.base_priority);
}

void test_timer_daemon_processes_expired_timers(void)
{
    xRTOS_Port_Ops_t custom_ops = xrtos_fake_port_ops;
    custom_ops.yield = test_timer_yield_jmp;

    // Reinitialize kernel
    (void)xRTOS_Kernel_Init(&s_kernel, &custom_ops);
    s_kernel.scheduler.is_started = true;

    // Force current task to be the timer daemon task for simulated execution context
    s_kernel.scheduler.current_task_id = xRTOS_TIMER_TASK_ID;

    // Initialize and start a software timer
    init_timer(0U, 5U, false);
    (void)xRTOS_Timer_Start(&s_timer);

    // Increment tick to 5 (expiry)
    bool should_yield = false;
    for (uint32_t i = 0; i < 5; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    // Verify semaphore count became 1 (since there was no waiter yet)
    TEST_ASSERT_EQUAL_UINT32(1U, s_kernel.timer_sem.count);

    // Run the daemon entry function (it consumes the semaphore token, processes timers, and then blocks/yields on second iteration)
    s_callback_count = 0;
    if (setjmp(s_timer_exit_jmp) == 0)
    {
        xrtos_timer_daemon_entry(NULL);
    }

    // Verify callback was executed in task context
    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 0U));
}

void test_timer_daemon_preemption_yield(void)
{
    xRTOS_Port_Ops_t custom_ops = xrtos_fake_port_ops;
    custom_ops.yield = test_timer_yield_jmp;

    // Reinitialize kernel
    (void)xRTOS_Kernel_Init(&s_kernel, &custom_ops);
    s_kernel.scheduler.is_started = true;

    // Create Idle task (required to check priorities)
    static xRTOS_Task_Context_t idle_ctx;
    static uint32_t idle_stack[64] = {0};
    xRTOS_Task_Config_t idle_cfg = {.task_id = xRTOS_IDLE_TASK_ID,
                                    .priority = xRTOS_IDLE_PRIORITY,
                                    .entry = counting_callback,
                                    .entry_arg = NULL,
                                    .stack_mem = idle_stack,
                                    .stack_words = 64};
    (void)xRTOS_Task_Create(&idle_ctx, &idle_cfg);

    // Manually block the Timer Daemon Task on the semaphore
    xRTOS_Bitmap_Set(&s_kernel.timer_sem.wait_map, xRTOS_TIMER_TASK_ID);
    s_kernel.timer_task_ctx.state = xRTOS_TASK_STATE_BLOCKED;
    s_kernel.timer_task_ctx.wait_map_ptr = &s_kernel.timer_sem.wait_map;
    xRTOS_Bitmap_Clear(&s_kernel.scheduler.ready_map, xRTOS_TIMER_TASK_ID);
    xRTOS_Bitmap_Set(&s_kernel.scheduler.blocked_map, xRTOS_TIMER_TASK_ID);

    // Set current task to Idle task
    s_kernel.scheduler.current_task_id = xRTOS_IDLE_TASK_ID;
    s_kernel.scheduler.current_priority = xRTOS_IDLE_PRIORITY;

    // Give the semaphore from ISR
    bool should_yield = false;
    xRETURN_t ret = xRTOS_Sem_Give_From_ISR(&s_kernel.timer_sem, &should_yield);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xRTOS_OK, ret);
    // Since timer task priority (1) is higher than idle task (31), preemption must be requested
    TEST_ASSERT_TRUE(should_yield);
    // Timer task must be READY
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_kernel.timer_task_ctx.state);
}
#endif

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Init
    RUN_TEST(test_timer_init_null_ctx);
    RUN_TEST(test_timer_init_null_config);
    RUN_TEST(test_timer_init_null_callback);
    RUN_TEST(test_timer_init_timer_id_out_of_range);
    RUN_TEST(test_timer_init_zero_period_returns_invalid_argument);
    RUN_TEST(test_timer_init_slot_occupied_returns_invalid_argument);
    RUN_TEST(test_timer_init_sets_callback);
    RUN_TEST(test_timer_init_sets_period_ticks);
    RUN_TEST(test_timer_init_sets_timer_id);
    RUN_TEST(test_timer_init_sets_is_periodic_true);
    RUN_TEST(test_timer_init_sets_is_periodic_false);
    RUN_TEST(test_timer_init_active_timers_map_bit_clear);
    RUN_TEST(test_timer_init_registers_in_timer_table);
    RUN_TEST(test_timer_init_masks_interrupts_while_registering);
    RUN_TEST(test_timer_init_passes_callback_arg);

    // Start
    RUN_TEST(test_timer_start_null_ctx);
    RUN_TEST(test_timer_start_unregistered_ctx_returns_invalid_argument);
    RUN_TEST(test_timer_start_sets_active_timers_map_bit);
    RUN_TEST(test_timer_start_computes_expiry_tick);
    RUN_TEST(test_timer_start_reset_deadline_when_already_active);
    RUN_TEST(test_timer_start_masks_interrupts_while_arming);

    // Stop
    RUN_TEST(test_timer_stop_null_ctx);
    RUN_TEST(test_timer_stop_unregistered_ctx_returns_invalid_argument);
    RUN_TEST(test_timer_stop_clears_active_timers_map_bit);
    RUN_TEST(test_timer_stop_masks_interrupts_while_disarming);

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_ISR)
    // Tick integration - one-shot
    RUN_TEST(test_timer_oneshot_callback_not_called_before_expiry);
    RUN_TEST(test_timer_oneshot_callback_called_at_expiry);
    RUN_TEST(test_timer_oneshot_deactivated_after_fire);
    RUN_TEST(test_timer_oneshot_callback_called_exactly_once);
    RUN_TEST(test_timer_oneshot_callback_receives_arg);

    // Tick integration - periodic
    RUN_TEST(test_timer_periodic_fires_at_first_period);
    RUN_TEST(test_timer_periodic_remains_active_after_fire);
    RUN_TEST(test_timer_periodic_reloads_expiry);
    RUN_TEST(test_timer_periodic_fires_multiple_times);

    // Stop prevents callback
    RUN_TEST(test_timer_stop_before_expiry_prevents_callback);

    // Multiple timers
    RUN_TEST(test_two_timers_fire_independently);
#endif

#if (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_TASK)
    RUN_TEST(test_timer_daemon_initializes_and_creates_task);
    RUN_TEST(test_timer_daemon_processes_expired_timers);
    RUN_TEST(test_timer_daemon_preemption_yield);
#endif

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
