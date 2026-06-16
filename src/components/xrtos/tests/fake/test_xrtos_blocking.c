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

// @file test_xrtos_blocking.c
// @brief Host tests for the xRTOS blocking model infrastructure (Phase 10).
//
// Tests verify:
//   1. xRTOS_Tick_Increment_From_ISR clears *wait_map_ptr when a task times out
//      while blocked on a sync object (non-NULL wait_map_ptr).
//   2. xRTOS_Scheduler_Unblock_From_Wait_Map implements the normal wake flow:
//      selects the highest-priority waiter, clears wait_map and timeout_map bits,
//      and unblocks the task with block_status OK.
//
// The "blocking flow" from Section 16 of the implementation plan is exercised
// by manually setting the relevant bitmaps (as a sync primitive would before
// calling Block_Current), then checking that both the ISR timeout path and the
// normal-wake path correctly clean up all bitmaps.
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

// A fake sync-object wait_map (stands in for a semaphore or mutex's wait_map).
static xRTOS_Bitmap_t s_wait_map;

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

// Set up a task as if it entered Block_Current with a wait_map and timeout.
// Mirrors what a semaphore take would do before calling Block_Current:
//   - set wait_map bit
//   - set timeout_map bit
//   - set task->wait_map_ptr
//   - set task->wake_tick
//   - call Block_Current(wait_map)
static void block_task_on_wait_map(xRTOS_Task_Context_t *task_ctx, xRTOS_Bitmap_t *wait_map, uint32_t timeout_ticks)
{
    uint32_t task_id = task_ctx->task_id;

    // Arm the timeout.
    task_ctx->wake_tick = s_kernel.tick_count + timeout_ticks;
    xRTOS_Bitmap_Set(&s_kernel.timeout_map, task_id);

    // Register the task in the sync object's waiter list.
    xRTOS_Bitmap_Set(wait_map, task_id);

    // Block the task (sets state=BLOCKED, clears ready_map bit, stores wait_map_ptr).
    (void)xRTOS_Scheduler_Block_Current(wait_map);
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);

    // Zero the fake wait_map before each test.
    s_wait_map.words[0] = 0U;
}

void tearDown(void)
{
}

// TESTS: ISR timeout path clears *wait_map_ptr ////////////////////////////////

void test_isr_timeout_clears_wait_map_bit(void)
{
    // task_a blocked on s_wait_map with a 3-tick timeout.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    // current = 10 (task_a running).

    block_task_on_wait_map(&s_task_a, &s_wait_map, 3U);
    // s_wait_map bit 10 set, timeout_map bit 10 set.

    // Switch to idle so ISR fires while idle is "running".
    (void)xRTOS_Scheduler_Select_Next(); // idle
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    // The ISR must have cleared task_a's bit from s_wait_map.
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_wait_map, 10U));
}

void test_isr_timeout_clears_timeout_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 3U);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_isr_timeout_unblocks_task_with_timeout_status(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.block_status = xRETURN_xRTOS_OK;
    block_task_on_wait_map(&s_task_a, &s_wait_map, 3U);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_TIMEOUT, s_task_a.block_status);
}

void test_isr_timeout_with_null_wait_map_ptr_still_works(void)
{
    // Pure Task_Delay (no sync object): wait_map_ptr is NULL.
    // Verifies the ISR null-guard does not crash or corrupt state.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Task_Delay(3U); // wait_map_ptr = NULL

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 3U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

// TESTS: xRTOS_Scheduler_Unblock_From_Wait_Map ////////////////////////////////

void test_unblock_from_empty_wait_map_returns_no_tasks_ready(void)
{
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    uint32_t priority = 0U;
    xRETURN_t ret = xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, &priority);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NO_TASKS_READY, ret);
}

void test_unblock_from_null_wait_map_returns_null_pointer(void)
{
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    xRETURN_t ret = xRTOS_Scheduler_Unblock_From_Wait_Map(NULL, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_unblock_from_wait_map_wakes_single_waiter(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    uint32_t woken = 0U;
    xRETURN_t ret = xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, &woken);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(10U, woken);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_unblock_from_wait_map_sets_block_status_ok(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    s_task_a.block_status = xRETURN_xERR_xRTOS_TIMEOUT; // pre-set to wrong value
    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, s_task_a.block_status);
}

void test_unblock_from_wait_map_clears_wait_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_wait_map, 10U));
}

void test_unblock_from_wait_map_clears_timeout_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_unblock_from_wait_map_clears_wait_map_ptr(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_NULL(s_task_a.wait_map_ptr);
}

void test_unblock_from_wait_map_picks_highest_priority_waiter(void)
{
    // task_a (priority 10) and task_b (priority 5) both wait on s_wait_map.
    // The waker should select priority 5 (numerically lower = higher priority).
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)register_task(&s_task_b, s_stack_b, 5U);
    (void)xRTOS_Kernel_Start();
    // current = 5 (task_b has highest priority).

    // Block task_b first (it's current).
    block_task_on_wait_map(&s_task_b, &s_wait_map, 100U);

    // Switch to task_a (next ready), then block task_a.
    (void)xRTOS_Scheduler_Select_Next(); // task_a (10) is next ready
    xRTOS_Scheduler_Switch();
    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    // Switch to idle, then wake one waiter.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    uint32_t woken = 0xFFFFFFFFU;
    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, &woken);

    // Priority 5 (task_b) is numerically lower - highest priority.
    TEST_ASSERT_EQUAL_UINT32(5U, woken);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
    // task_a should still be blocked.
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
    // wait_map bit 10 still set; bit 5 cleared.
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_wait_map, 10U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_wait_map, 5U));
}

void test_unblock_from_wait_map_picks_highest_priority_with_distinct_task_ids(void)
{
    (void)register_idle();
    (void)register_task_with_id(&s_task_a, s_stack_a, 11U, 10U);
    (void)register_task_with_id(&s_task_b, s_stack_b, 5U, 3U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_b, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    uint32_t woken = xRTOS_INVALID_TASK_ID;
    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, &woken);

    TEST_ASSERT_EQUAL_UINT32(5U, woken);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_wait_map, 11U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_wait_map, 5U));
}

void test_unblock_from_wait_map_sets_schedule_pending_for_higher_priority(void)
{
    // task_a (priority 10) blocks; then idle runs. Waking task_a should set
    // is_schedule_pending because 10 < current_priority (31 = idle).
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    (void)xRTOS_Scheduler_Select_Next(); // idle
    xRTOS_Scheduler_Switch();
    // current_priority = 31, is_schedule_pending cleared by Switch.

    (void)xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
}

void test_unblock_from_wait_map_unblocked_priority_null_is_safe(void)
{
    // Passing NULL for unblocked_priority should not crash.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();

    block_task_on_wait_map(&s_task_a, &s_wait_map, 100U);

    xRETURN_t ret = xRTOS_Scheduler_Unblock_From_Wait_Map(&s_wait_map, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_isr_timeout_clears_wait_map_bit);
    RUN_TEST(test_isr_timeout_clears_timeout_map_bit);
    RUN_TEST(test_isr_timeout_unblocks_task_with_timeout_status);
    RUN_TEST(test_isr_timeout_with_null_wait_map_ptr_still_works);

    RUN_TEST(test_unblock_from_empty_wait_map_returns_no_tasks_ready);
    RUN_TEST(test_unblock_from_null_wait_map_returns_null_pointer);
    RUN_TEST(test_unblock_from_wait_map_wakes_single_waiter);
    RUN_TEST(test_unblock_from_wait_map_sets_block_status_ok);
    RUN_TEST(test_unblock_from_wait_map_clears_wait_map_bit);
    RUN_TEST(test_unblock_from_wait_map_clears_timeout_map_bit);
    RUN_TEST(test_unblock_from_wait_map_clears_wait_map_ptr);
    RUN_TEST(test_unblock_from_wait_map_picks_highest_priority_waiter);
    RUN_TEST(test_unblock_from_wait_map_picks_highest_priority_with_distinct_task_ids);
    RUN_TEST(test_unblock_from_wait_map_sets_schedule_pending_for_higher_priority);
    RUN_TEST(test_unblock_from_wait_map_unblocked_priority_null_is_safe);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
