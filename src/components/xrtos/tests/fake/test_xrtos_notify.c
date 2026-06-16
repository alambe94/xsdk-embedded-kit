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

// @file test_xrtos_notify.c
// @brief Host tests for xRTOS task notifications (Phase 11).
//
// Tests cover three areas:
//   1. xRTOS_Task_Notify - latches value, sets pending, wakes blocked waiter.
//   2. xRTOS_Task_Notify_From_ISR - same as Notify plus should_yield output.
//   3. xRTOS_Task_Notify_Wait - immediate return when pending, WOULD_BLOCK on
//      zero timeout, blocking setup (notify_wait_map / timeout_map arming),
//      clear_on_entry / clear_on_exit semantics.
//
// Host simulation note:
//   xRTOS_Scheduler_Block_Current does not actually switch context on the host.
//   For tests that require the task to be in the "blocked in Notify_Wait" state,
//   enter_notify_wait() manually arms the bitmap state and calls Block_Current
//   directly (identical to what Notify_Wait does before blocking). Notify is
//   then called and the resulting state is verified.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_notify.h"
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

static void dummy_entry(void *arg)
{
    (void)arg;
}

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

static xRETURN_t register_task(xRTOS_Task_Context_t *ctx, uint32_t *stack, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = priority;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(ctx, &cfg);
}

static xRETURN_t register_task_with_id(xRTOS_Task_Context_t *ctx, uint32_t *stack, uint32_t task_id, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = task_id;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(ctx, &cfg);
}

// Manually put task_ctx into the "blocked in Notify_Wait" state, mirroring
// what xRTOS_Task_Notify_Wait does just before Block_Current.
// clear_on_exit and timeout_ticks match the Notify_Wait parameters.
static void enter_notify_wait(xRTOS_Task_Context_t *task_ctx, uint32_t clear_on_exit, uint32_t timeout_ticks)
{
    uint32_t task_id = task_ctx->task_id;

    task_ctx->block_payload.notify.clear_on_exit = clear_on_exit;
    task_ctx->block_payload.notify.value = 0U;
    task_ctx->wake_tick = s_kernel.tick_count + timeout_ticks;

    xRTOS_Bitmap_Set(&s_kernel.notify_wait_map, task_id);

    if (timeout_ticks != xRTOS_WAIT_FOREVER)
    {
        xRTOS_Bitmap_Set(&s_kernel.timeout_map, task_id);
    }

    (void)xRTOS_Scheduler_Block_Current(&s_kernel.notify_wait_map);
    // On host: Block_Current returns immediately; task is now BLOCKED and
    // notify_wait_map bit is set - exactly what Notify / ISR expects to find.
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: xRTOS_Task_Notify - value and pending latch //////////////////////////

void test_notify_sets_pending_flag(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    (void)xRTOS_Task_Notify(10U, 0U);

    TEST_ASSERT_TRUE(s_task_a.has_notify_pending);
}

void test_notify_ors_value_into_notify_value(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0x0FU;
    (void)xRTOS_Task_Notify(10U, 0xF0U);

    TEST_ASSERT_EQUAL_UINT32(0xFFU, s_task_a.notify_value);
}

void test_notify_accumulates_across_multiple_calls(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0U;
    (void)xRTOS_Task_Notify(10U, 0x01U);
    (void)xRTOS_Task_Notify(10U, 0x02U);
    (void)xRTOS_Task_Notify(10U, 0x04U);

    TEST_ASSERT_EQUAL_UINT32(0x07U, s_task_a.notify_value);
}

void test_notify_returns_invalid_argument_for_unknown_task_id(void)
{
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    // task_id 5 is not registered.
    xRETURN_t ret = xRTOS_Task_Notify(5U, 0U);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_notify_does_not_wake_running_task(void)
{
    // When the target task is RUNNING (not blocked in Notify_Wait), Notify
    // latches the value but does not change the task's state.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    // task_a is the current running task (priority 10 < idle 31).

    (void)xRTOS_Task_Notify(10U, 0xDEADU);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_RUNNING, s_task_a.state);
    TEST_ASSERT_EQUAL_UINT32(0xDEADU, s_task_a.notify_value);
}

// TESTS: xRTOS_Task_Notify - waking a blocked waiter //////////////////////////

void test_notify_targets_task_id_not_priority(void)
{
    (void)register_idle();
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();

    xRETURN_t ret = xRTOS_Task_Notify(9U, 0x55U);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_TRUE(s_task_a.has_notify_pending);
    TEST_ASSERT_EQUAL_UINT32(0x55U, s_task_a.notify_value);
}

void test_notify_wakes_blocked_task(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);
    // task_a BLOCKED in notify wait.

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_notify_clears_notify_wait_map_bit_on_wake(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.notify_wait_map, 10U));
}

void test_notify_clears_timeout_map_bit_on_wake(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, 50U); // finite timeout -> timeout_map armed

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_notify_sets_block_status_ok_on_wake(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.block_status = xRETURN_xERR_xRTOS_TIMEOUT; // pre-set to wrong value
    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, s_task_a.block_status);
}

void test_notify_clears_wait_map_ptr_on_wake(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_NULL(s_task_a.wait_map_ptr);
}

void test_notify_applies_clear_on_exit_to_notify_value_on_wake(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    // clear_on_exit = 0xF0: upper nibble should be cleared after wake.
    enter_notify_wait(&s_task_a, 0xF0U, xRTOS_WAIT_FOREVER);

    s_task_a.notify_value = 0U;
    (void)xRTOS_Task_Notify(10U, 0xFFU); // OR 0xFF; then clear 0xF0 -> 0x0F

    TEST_ASSERT_EQUAL_UINT32(0x0FU, s_task_a.notify_value);
}

void test_notify_latches_pre_clear_value_for_blocked_waiter(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0xF0U, xRTOS_WAIT_FOREVER);

    s_task_a.notify_value = 0U;
    (void)xRTOS_Task_Notify(10U, 0xFFU);

    TEST_ASSERT_EQUAL_UINT32(0xFFU, s_task_a.block_payload.notify.value);
    TEST_ASSERT_EQUAL_UINT32(0x0FU, s_task_a.notify_value);
}

void test_notify_clears_pending_after_waking(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Task_Notify(10U, 0x01U);

    // After waking, pending should be cleared (notification was consumed).
    TEST_ASSERT_FALSE(s_task_a.has_notify_pending);
}

void test_notify_sets_schedule_pending_for_higher_priority_waiter(void)
{
    // task_a (priority 10) waits; idle (31) is current. Notifying task_a
    // should set is_schedule_pending because 10 < 31.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    // Switch to idle so it's the current task.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch(); // current = 31, is_schedule_pending cleared.

    (void)xRTOS_Task_Notify(10U, 0x01U);

    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
}

// TESTS: xRTOS_Task_Notify_From_ISR ///////////////////////////////////////////

void test_notify_from_isr_sets_should_yield_true_for_higher_priority(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch(); // current = 31 (idle).

    bool should_yield = false;
    (void)xRTOS_Task_Notify_From_ISR(10U, 0x01U, &should_yield);

    TEST_ASSERT_TRUE(should_yield);
}

void test_notify_from_isr_wakes_blocked_task(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    enter_notify_wait(&s_task_a, 0U, xRTOS_WAIT_FOREVER);

    bool should_yield = false;
    (void)xRTOS_Task_Notify_From_ISR(10U, 0xBEEFU, &should_yield);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

// TESTS: xRTOS_Task_Notify_Wait - immediate-return paths //////////////////////

void test_notify_wait_returns_ok_immediately_when_pending(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0xCAFEU;
    s_task_a.has_notify_pending = true;

    xRETURN_t ret = xRTOS_Task_Notify_Wait(0U, 0U, NULL, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
}

void test_notify_wait_clears_pending_flag_on_immediate_return(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = true;

    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_FALSE(s_task_a.has_notify_pending);
}

void test_notify_wait_returns_value_when_pending(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0xABCDU;
    s_task_a.has_notify_pending = true;

    uint32_t out = 0U;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, &out, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(0xABCDU, out);
}

void test_notify_wait_applies_clear_on_entry_before_checking_pending(void)
{
    // clear_on_entry masks bits BEFORE the pending check - even pending
    // notifications arrive with already-masked bits.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0xFFU;
    s_task_a.has_notify_pending = true;

    uint32_t out = 0U;
    // clear_on_entry = 0xF0: upper nibble cleared before reading.
    (void)xRTOS_Task_Notify_Wait(0xF0U, 0U, &out, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(0x0FU, out);
}

void test_notify_wait_applies_clear_on_exit_on_immediate_return(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.notify_value = 0xFFU;
    s_task_a.has_notify_pending = true;

    uint32_t out = 0U;
    // clear_on_exit = 0x0F: lower nibble cleared after reading.
    (void)xRTOS_Task_Notify_Wait(0U, 0x0FU, &out, xRTOS_WAIT_FOREVER);

    // out should have the value BEFORE clear_on_exit.
    TEST_ASSERT_EQUAL_UINT32(0xFFU, out);
    // notify_value should have clear_on_exit applied.
    TEST_ASSERT_EQUAL_UINT32(0xF0U, s_task_a.notify_value);
}

void test_notify_wait_returns_would_block_when_not_pending_and_zero_timeout(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;

    xRETURN_t ret = xRTOS_Task_Notify_Wait(0U, 0U, NULL, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, ret);
}

// TESTS: xRTOS_Task_Notify_Wait - blocking setup //////////////////////////////

void test_notify_wait_blocks_task_when_not_pending(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;

    // On host: Block_Current returns immediately but state is set to BLOCKED.
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, 10U);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_notify_wait_sets_notify_wait_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, 10U);

    // notify_wait_map bit for task_a (priority 10) must be set.
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.notify_wait_map, 10U));
}

void test_notify_wait_uses_task_id_for_wait_maps(void)
{
    (void)register_idle();
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, 10U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.notify_wait_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.notify_wait_map, 3U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 3U));
}

void test_notify_wait_arms_timeout_map_for_finite_timeout(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, 50U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_notify_wait_does_not_arm_timeout_map_for_wait_forever(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_notify_wait_timeout_path_fires_via_isr(void)
{
    // After blocking in Notify_Wait, simulate the timeout firing via the ISR.
    // The ISR should clear notify_wait_map and unblock the task with TIMEOUT.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.has_notify_pending = false;
    s_task_a.block_status = xRETURN_xRTOS_OK;
    (void)xRTOS_Task_Notify_Wait(0U, 0U, NULL, 5U);
    // task_a BLOCKED, notify_wait_map[10] set, timeout_map[10] set.

    // Switch to idle so ISR fires while idle runs.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    // Task should be READY with TIMEOUT status; notify_wait_map must be clear.
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_TIMEOUT, s_task_a.block_status);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.notify_wait_map, 10U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_notify_sets_pending_flag);
    RUN_TEST(test_notify_ors_value_into_notify_value);
    RUN_TEST(test_notify_accumulates_across_multiple_calls);
    RUN_TEST(test_notify_returns_invalid_argument_for_unknown_task_id);
    RUN_TEST(test_notify_does_not_wake_running_task);
    RUN_TEST(test_notify_targets_task_id_not_priority);

    RUN_TEST(test_notify_wakes_blocked_task);
    RUN_TEST(test_notify_clears_notify_wait_map_bit_on_wake);
    RUN_TEST(test_notify_clears_timeout_map_bit_on_wake);
    RUN_TEST(test_notify_sets_block_status_ok_on_wake);
    RUN_TEST(test_notify_clears_wait_map_ptr_on_wake);
    RUN_TEST(test_notify_applies_clear_on_exit_to_notify_value_on_wake);
    RUN_TEST(test_notify_latches_pre_clear_value_for_blocked_waiter);
    RUN_TEST(test_notify_clears_pending_after_waking);
    RUN_TEST(test_notify_sets_schedule_pending_for_higher_priority_waiter);

    RUN_TEST(test_notify_from_isr_sets_should_yield_true_for_higher_priority);
    RUN_TEST(test_notify_from_isr_wakes_blocked_task);

    RUN_TEST(test_notify_wait_returns_ok_immediately_when_pending);
    RUN_TEST(test_notify_wait_clears_pending_flag_on_immediate_return);
    RUN_TEST(test_notify_wait_returns_value_when_pending);
    RUN_TEST(test_notify_wait_applies_clear_on_entry_before_checking_pending);
    RUN_TEST(test_notify_wait_applies_clear_on_exit_on_immediate_return);
    RUN_TEST(test_notify_wait_returns_would_block_when_not_pending_and_zero_timeout);

    RUN_TEST(test_notify_wait_blocks_task_when_not_pending);
    RUN_TEST(test_notify_wait_sets_notify_wait_map_bit);
    RUN_TEST(test_notify_wait_uses_task_id_for_wait_maps);
    RUN_TEST(test_notify_wait_arms_timeout_map_for_finite_timeout);
    RUN_TEST(test_notify_wait_does_not_arm_timeout_map_for_wait_forever);
    RUN_TEST(test_notify_wait_timeout_path_fires_via_isr);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
