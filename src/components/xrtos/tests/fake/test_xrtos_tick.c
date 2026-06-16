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

// @file test_xrtos_tick.c
// @brief Host tests for the xRTOS tick counter and timeout layer (Phase 9).
//
// Tests verify:
//   1. xRTOS_Tick_Get returns the kernel tick counter.
//   2. xRTOS_Task_Delay arms wake_tick and the timeout_map, then blocks the task.
//   3. xRTOS_Tick_Increment_From_ISR expires timeouts at the correct tick and
//      sets should_yield correctly.
//   4. xRTOS_Tick_Has_Expired handles 32-bit wrap correctly.
//
// Host simulation note: the fake port does not implement a real context switch,
// so xRTOS_Task_Delay returns immediately after setting up the timeout and
// calling Block_Current. Tests therefore check the in-memory state directly
// rather than re-entering from the blocked task's perspective.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;

static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_a;
static uint32_t s_stack_a[64U];

static xRTOS_Task_Context_t s_task_b;
static uint32_t s_stack_b[64U];

static uint32_t s_yield_count;
static uint32_t s_disable_count;
static uint32_t s_enable_count;

static void dummy_entry(void *arg)
{
    (void)arg;
}

static void counting_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg)
{
    (void)entry;
    (void)arg;
    task_ctx->stack_top = task_ctx->stack_mem + task_ctx->stack_words;
}

static void counting_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    (void)task_ctx;
}

static void counting_yield(void)
{
    s_yield_count++;
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

static xRETURN_t register_idle(void)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = xRTOS_IDLE_TASK_ID;
    cfg.priority = xRTOS_IDLE_PRIORITY;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_idle_stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(&s_idle_ctx, &cfg);
}

static xRETURN_t register_task_a(uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = priority;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_stack_a;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(&s_task_a, &cfg);
}

static xRETURN_t register_task_b(uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = priority;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_stack_b;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(&s_task_b, &cfg);
}

void setUp(void)
{
    s_yield_count = 0U;
    s_disable_count = 0U;
    s_enable_count = 0U;
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: xRTOS_Tick_Get ///////////////////////////////////////////////////////

void test_tick_get_returns_zero_after_init(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, xRTOS_Tick_Get());
}

void test_tick_get_tracks_increments(void)
{
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    bool dummy = false;
    xRTOS_Tick_Increment_From_ISR(&dummy);
    xRTOS_Tick_Increment_From_ISR(&dummy);
    xRTOS_Tick_Increment_From_ISR(&dummy);

    TEST_ASSERT_EQUAL_UINT32(3U, xRTOS_Tick_Get());
}

// TESTS: xRTOS_Task_Delay /////////////////////////////////////////////////////

void test_task_delay_returns_invalid_state_before_start(void)
{
    // Kernel initialized but not started - delay must reject.
    (void)register_idle();
    (void)register_task_a(10U);
    // No xRTOS_Kernel_Start call.
    xRETURN_t ret = xRTOS_Task_Delay(5U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, ret);
}

void test_task_delay_zero_sets_schedule_pending_without_blocking(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();
    // current task is task_a (priority 10).

    s_kernel.scheduler.is_schedule_pending = false;

    xRETURN_t ret = xRTOS_Task_Delay(0U);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
    // Task must NOT be blocked.
    TEST_ASSERT_NOT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
    // Timeout map must be clear.
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_task_delay_zero_calls_port_yield(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(0U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_yield_count);
}

void test_task_delay_zero_masks_interrupts(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(0U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_disable_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_enable_count);
}

void test_task_delay_sets_wake_tick(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    xRETURN_t ret = xRTOS_Task_Delay(25U);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(25U, s_task_a.wake_tick);
}

void test_task_delay_arms_timeout_map(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(10U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_task_delay_blocks_current_task(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(10U);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_task_delay_blocking_calls_port_yield(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(10U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_yield_count);
}

void test_task_delay_blocking_masks_interrupts(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &s_counting_port_ops);
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(10U);

    TEST_ASSERT_EQUAL_UINT32(1U, s_disable_count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_enable_count);
}

void test_task_delay_wait_map_ptr_is_null(void)
{
    // Delay is a pure timed wait - no sync object associated.
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(10U);

    TEST_ASSERT_NULL(s_task_a.wait_map_ptr);
}

// TESTS: xRTOS_Tick_Increment_From_ISR - expiry ///////////////////////////////

void test_tick_does_not_unblock_before_expiry(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(5U);
    // task_a BLOCKED, wake_tick = 5.

    bool should_yield = false;
    // Fire 4 ticks - one short of wake_tick.
    for (uint32_t i = 0U; i < 4U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_tick_unblocks_task_at_expiry(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(5U);

    bool should_yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_tick_clears_timeout_map_at_expiry(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(5U);

    bool should_yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_tick_sets_block_status_timeout_on_expiry(void)
{
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.block_status = xRETURN_xRTOS_OK;
    (void)xRTOS_Task_Delay(3U);

    bool should_yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_TIMEOUT, s_task_a.block_status);
}

void test_tick_advances_count_even_without_timeouts(void)
{
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    bool dummy = false;
    for (uint32_t i = 0U; i < 100U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&dummy);
    }

    TEST_ASSERT_EQUAL_UINT32(100U, xRTOS_Tick_Get());
}

// TESTS: should_yield /////////////////////////////////////////////////////////

void test_tick_should_yield_true_when_higher_priority_expires(void)
{
    // task_a at priority 10 blocks for 5 ticks.
    // After simulating a switch to idle, when task_a expires the ISR should
    // signal a yield because priority 10 < current_priority (31 = idle).
    (void)register_idle();
    (void)register_task_a(10U);
    (void)xRTOS_Kernel_Start();
    // current_priority = 10 (task_a is highest priority).

    (void)xRTOS_Task_Delay(5U);
    // task_a BLOCKED; is_schedule_pending = true.

    // Simulate context switch to idle (what the OS would do on real hardware).
    (void)xRTOS_Scheduler_Select_Next(); // selects idle (31)
    xRTOS_Scheduler_Switch();            // current_priority = 31, is_schedule_pending cleared.

    // Now simulate the 5 ticks.
    bool should_yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    // task_a (priority 10) < current_priority (31) -> Unblock set is_schedule_pending.
    TEST_ASSERT_TRUE(should_yield);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_tick_should_yield_false_when_no_timeout_fired(void)
{
    // No tasks with timeouts; should_yield starts and stays false.
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    bool should_yield = true; // Pre-set to true to detect a wrong clear.
    s_kernel.scheduler.is_schedule_pending = false;
    xRTOS_Tick_Increment_From_ISR(&should_yield);

    TEST_ASSERT_FALSE(should_yield);
}

void test_tick_expires_multiple_tasks_in_one_tick(void)
{
    // task_a (priority 10) and task_b (priority 20) both expire at tick 3.
    (void)register_idle();
    (void)register_task_a(10U);
    (void)register_task_b(20U);
    (void)xRTOS_Kernel_Start();
    // current_priority = 10 (task_a running).

    // Block task_a for 3 ticks.
    (void)xRTOS_Task_Delay(3U);
    // task_a BLOCKED.

    // Simulate a switch so task_b is now current; then block task_b for 3 ticks too.
    (void)xRTOS_Scheduler_Select_Next(); // selects task_b (20) or idle.
    xRTOS_Scheduler_Switch();
    (void)xRTOS_Task_Delay(3U);

    // Simulate switch to idle so both are blocked.
    (void)xRTOS_Scheduler_Select_Next(); // selects idle (31).
    xRTOS_Scheduler_Switch();

    // Fire 3 ticks - both should unblock in the same tick.
    bool should_yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 20U));
    TEST_ASSERT_TRUE(should_yield);
}

void test_tick_same_priority_periodic_delays_alternate_workers(void)
{
    // Mirrors the CH32H417 bring-up app: two worker tasks share priority and
    // delay period, so each expiry cycle should resume A, then B.
    xRTOS_Task_Config_t cfg_a = {0U, 5U, dummy_entry, NULL, s_stack_a, 64U, NULL};
    xRTOS_Task_Config_t cfg_b = {1U, 5U, dummy_entry, NULL, s_stack_b, 64U, NULL};
    (void)xRTOS_Task_Create(&s_task_a, &cfg_a);
    (void)xRTOS_Task_Create(&s_task_b, &cfg_b);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    TEST_ASSERT_EQUAL_UINT32(0U, s_kernel.scheduler.current_task_id);

    (void)xRTOS_Task_Delay(1000U);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(1U, s_kernel.scheduler.current_task_id);

    (void)xRTOS_Task_Delay(1000U);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(xRTOS_IDLE_TASK_ID, s_kernel.scheduler.current_task_id);

    bool should_yield = false;
    for (uint32_t i = 0U; i < 1000U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&should_yield);
    }

    TEST_ASSERT_TRUE(should_yield);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(0U, s_kernel.scheduler.current_task_id);

    (void)xRTOS_Task_Delay(1000U);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(1U, s_kernel.scheduler.current_task_id);
}

// TESTS: xRTOS_Tick_Has_Expired ///////////////////////////////////////////////

void test_tick_has_expired_returns_true_at_exact_tick(void)
{
    TEST_ASSERT_TRUE(xRTOS_Tick_Has_Expired(10U, 10U));
}

void test_tick_has_expired_returns_true_past_wake_tick(void)
{
    TEST_ASSERT_TRUE(xRTOS_Tick_Has_Expired(11U, 10U));
}

void test_tick_has_expired_returns_false_before_wake_tick(void)
{
    TEST_ASSERT_FALSE(xRTOS_Tick_Has_Expired(9U, 10U));
}

void test_tick_has_expired_wraps_correctly(void)
{
    // tick_count wrapped past UINT32_MAX; wake_tick is just past the wrap.
    // tick_count = 3, wake_tick = UINT32_MAX - 1 (delay started near max).
    // (int32_t)(3 - 0xFFFFFFFE) = (int32_t)(0x00000005) = 5 >= 0 -> expired.
    TEST_ASSERT_TRUE(xRTOS_Tick_Has_Expired(3U, 0xFFFFFFFEU));
}

void test_tick_has_expired_not_expired_before_wrap(void)
{
    // Near wrap; tick_count = 0xFFFFFFFD, wake_tick = 0xFFFFFFFF (not yet).
    // (int32_t)(0xFFFFFFFD - 0xFFFFFFFF) = (int32_t)(0xFFFFFFFE) = -2 < 0 -> not expired.
    TEST_ASSERT_FALSE(xRTOS_Tick_Has_Expired(0xFFFFFFFDU, 0xFFFFFFFFU));
}

// TESTS: Round-Robin tick behaviour ///////////////////////////////////////////
//
// These tests exercise the xRTOS_CONFIG_ROUND_ROBIN_ENABLE path in
// xRTOS_Tick_Increment_From_ISR.  They are compiled unconditionally so the
// test binary always builds; the assertions are gated at run-time by checking
// whether RR is on or off, so results are correct under both build configs.

void test_tick_rr_no_schedule_pending_when_no_peer_at_current_priority(void)
{
    // Single task at its priority - no peer in ready_head, so the RR path
    // must NOT set is_schedule_pending regardless of RR setting.
    xRTOS_Task_Config_t cfg_a = {2U, 5U, dummy_entry, NULL, s_stack_a, 64U, NULL};
    (void)xRTOS_Task_Create(&s_task_a, &cfg_a);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    // task_a (priority 5) is now running; ready_head[5] is NULL (no peer).
    TEST_ASSERT_NULL(s_kernel.scheduler.ready_head[5U]);
    s_kernel.scheduler.is_schedule_pending = false;

    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);

    // No peer waiting - RR path must not fire regardless of RR setting.
    TEST_ASSERT_FALSE(s_kernel.scheduler.is_schedule_pending);
}

void test_tick_rr_sets_schedule_pending_when_peer_is_ready(void)
{
    // Two tasks at the same priority (task_id differs). After start, the second
    // stays in ready_head[5]. RR should request a switch; non-RR should not.
    xRTOS_Task_Config_t cfg_a = {2U, 5U, dummy_entry, NULL, s_stack_a, 64U, NULL};
    xRTOS_Task_Config_t cfg_b = {3U, 5U, dummy_entry, NULL, s_stack_b, 64U, NULL};
    (void)xRTOS_Task_Create(&s_task_a, &cfg_a);
    (void)xRTOS_Task_Create(&s_task_b, &cfg_b);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    // task_a (task_id=2) is running; task_b (task_id=3) is in ready_head[5].
    TEST_ASSERT_EQUAL_UINT32(2U, s_kernel.scheduler.current_task_id);
    TEST_ASSERT_NOT_NULL(s_kernel.scheduler.ready_head[5U]);
    s_kernel.scheduler.is_schedule_pending = false;

    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);

#if xRTOS_CONFIG_ROUND_ROBIN_ENABLE
    // Peer is waiting at the same priority - RR must request a switch.
    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
    TEST_ASSERT_TRUE(yield);
#else
    // RR disabled - tick alone must NOT set is_schedule_pending.
    TEST_ASSERT_FALSE(s_kernel.scheduler.is_schedule_pending);
    TEST_ASSERT_FALSE(yield);
#endif
}

// TESTS: defensive NULL-slot cleanup in tick ISR //////////////////////////////

void test_tick_isr_stale_timeout_bit_for_null_task_is_cleaned(void)
{
    // Put task_id=5 in the timeout_map without registering the task.
    // The tick ISR defensive guard must clear the stale bit without crashing.
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    xRTOS_Bitmap_Set(&s_kernel.timeout_map, 5U);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 5U));

    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 5U));
}

#if (xRTOS_MAX_TIMERS > 0U) && (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_ISR)
void test_tick_isr_stale_active_timer_bit_for_null_timer_is_cleaned(void)
{
    // Put timer_id=2 in the active_timers_map without registering the timer.
    // The tick ISR defensive guard must clear the stale bit without crashing.
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    xRTOS_Bitmap_Set(&s_kernel.active_timers_map, 2U);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 2U));

    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.active_timers_map, 2U));
}
#endif

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_tick_get_returns_zero_after_init);
    RUN_TEST(test_tick_get_tracks_increments);

    RUN_TEST(test_task_delay_returns_invalid_state_before_start);
    RUN_TEST(test_task_delay_zero_sets_schedule_pending_without_blocking);
    RUN_TEST(test_task_delay_zero_calls_port_yield);
    RUN_TEST(test_task_delay_zero_masks_interrupts);
    RUN_TEST(test_task_delay_sets_wake_tick);
    RUN_TEST(test_task_delay_arms_timeout_map);
    RUN_TEST(test_task_delay_blocks_current_task);
    RUN_TEST(test_task_delay_blocking_calls_port_yield);
    RUN_TEST(test_task_delay_blocking_masks_interrupts);
    RUN_TEST(test_task_delay_wait_map_ptr_is_null);

    RUN_TEST(test_tick_does_not_unblock_before_expiry);
    RUN_TEST(test_tick_unblocks_task_at_expiry);
    RUN_TEST(test_tick_clears_timeout_map_at_expiry);
    RUN_TEST(test_tick_sets_block_status_timeout_on_expiry);
    RUN_TEST(test_tick_advances_count_even_without_timeouts);

    RUN_TEST(test_tick_should_yield_true_when_higher_priority_expires);
    RUN_TEST(test_tick_should_yield_false_when_no_timeout_fired);
    RUN_TEST(test_tick_expires_multiple_tasks_in_one_tick);
    RUN_TEST(test_tick_same_priority_periodic_delays_alternate_workers);

    RUN_TEST(test_tick_has_expired_returns_true_at_exact_tick);
    RUN_TEST(test_tick_has_expired_returns_true_past_wake_tick);
    RUN_TEST(test_tick_has_expired_returns_false_before_wake_tick);
    RUN_TEST(test_tick_has_expired_wraps_correctly);
    RUN_TEST(test_tick_has_expired_not_expired_before_wrap);

    // Round-robin / cooperative scheduling (behaviour adapts to RR flag)
    RUN_TEST(test_tick_rr_no_schedule_pending_when_no_peer_at_current_priority);
    RUN_TEST(test_tick_rr_sets_schedule_pending_when_peer_is_ready);

    RUN_TEST(test_tick_isr_stale_timeout_bit_for_null_task_is_cleaned);
#if (xRTOS_MAX_TIMERS > 0U) && (xRTOS_CONFIG_TIMER_METHOD == xRTOS_TIMER_METHOD_ISR)
    RUN_TEST(test_tick_isr_stale_active_timer_bit_for_null_timer_is_cleaned);
#endif

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
