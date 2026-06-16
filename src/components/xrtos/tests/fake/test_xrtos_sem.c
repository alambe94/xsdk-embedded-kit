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

// @file test_xrtos_sem.c
// @brief Host tests for xRTOS semaphore (Phase 12).
//
// Tests cover:
//   1. xRTOS_Sem_Init  - field initialization and argument validation.
//   2. xRTOS_Sem_Take  - fast path (count > 0), WOULD_BLOCK on NO_WAIT,
//      blocking setup (wait_map / timeout_map arming).
//   3. xRTOS_Sem_Give  - count increment, waiter wake path, RESOURCE_FULL.
//   4. xRTOS_Sem_Give_From_ISR - ISR wake with should_yield output.
//   5. End-to-end timeout via the tick ISR.
//
// Host simulation note:
//   xRTOS_Scheduler_Block_Current does not perform a real context switch on the
//   host. xRTOS_Sem_Take with count == 0 transitions the current task to BLOCKED
//   and returns immediately with the default block_status (OK). Tests that verify
//   blocking SETUP check intermediate bitmap and state fields after the Take call
//   returns. Wake-path tests call xRTOS_Sem_Give/Give_From_ISR immediately after
//   and verify the resulting state.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;

static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_a;
static uint32_t s_stack_a[64U];

static xRTOS_Sem_Context_t s_sem;

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

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: xRTOS_Sem_Init ///////////////////////////////////////////////////////

void test_sem_init_sets_count(void)
{
    (void)xRTOS_Sem_Init(&s_sem, 2U, 5U, NULL);

    TEST_ASSERT_EQUAL_UINT32(2U, s_sem.count);
}

void test_sem_init_sets_max_count(void)
{
    (void)xRTOS_Sem_Init(&s_sem, 0U, 5U, NULL);

    TEST_ASSERT_EQUAL_UINT32(5U, s_sem.max_count);
}

void test_sem_init_clears_wait_map(void)
{
    // Pre-corrupt wait_map to verify Init zeroes it.
    s_sem.wait_map.words[0] = 0xFFFFFFFFU;
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_sem.wait_map));
}

void test_sem_init_rejects_null(void)
{
    xRETURN_t ret = xRTOS_Sem_Init(NULL, 0U, 1U, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, ret);
}

void test_sem_init_rejects_zero_max_count(void)
{
    xRETURN_t ret = xRTOS_Sem_Init(&s_sem, 0U, 0U, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_sem_init_rejects_initial_exceeds_max(void)
{
    xRETURN_t ret = xRTOS_Sem_Init(&s_sem, 5U, 3U, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_sem_init_binary_available(void)
{
    // Binary semaphore with one token available.
    xRETURN_t ret = xRTOS_Sem_Init(&s_sem, 1U, 1U, NULL);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, s_sem.count);
    TEST_ASSERT_EQUAL_UINT32(1U, s_sem.max_count);
}

// TESTS: xRTOS_Sem_Take - fast path ///////////////////////////////////////////

void test_sem_take_returns_ok_when_available(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 1U, 1U, NULL);

    xRETURN_t ret = xRTOS_Sem_Take(&s_sem, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
}

void test_sem_take_decrements_count(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 3U, 5U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(2U, s_sem.count);
}

void test_sem_take_returns_would_block_no_wait_when_empty(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    xRETURN_t ret = xRTOS_Sem_Take(&s_sem, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, ret);
}

// TESTS: xRTOS_Sem_Take - blocking setup //////////////////////////////////////

void test_sem_take_blocks_task(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    // Block_Current returns immediately on host; state remains BLOCKED.
    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_sem_take_sets_wait_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_sem.wait_map, 10U));
}

void test_sem_take_uses_task_id_for_wait_map_bit(void)
{
    (void)register_idle();
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_sem.wait_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_sem.wait_map, 3U));
}

void test_sem_take_arms_timeout_map_finite(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, 100U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_sem_take_does_not_arm_timeout_map_wait_forever(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 10U));
}

void test_sem_take_resets_stale_block_status_when_blocking(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    s_task_a.block_status = xRETURN_xERR_xRTOS_TIMEOUT;

    xRETURN_t ret = xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, s_task_a.block_status);
}

// TESTS: xRTOS_Sem_Give - no waiter paths /////////////////////////////////////

void test_sem_give_increments_count(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 3U, NULL);

    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL_UINT32(1U, s_sem.count);
}

void test_sem_give_returns_ok(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    xRETURN_t ret = xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
}

void test_sem_give_returns_full_at_max(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 1U, 1U, NULL); // binary, already at max

    xRETURN_t ret = xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_RESOURCE_FULL, ret);
}

// TESTS: xRTOS_Sem_Give - waiter wake path ////////////////////////////////////

void test_sem_give_wakes_blocked_task(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER); // task_a now BLOCKED
    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_sem_give_sets_block_status_ok(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, s_task_a.block_status);
}

void test_sem_give_clears_wait_map_bit(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_sem.wait_map, 10U));
}

void test_sem_give_does_not_increment_count_when_waking_waiter(void)
{
    // Give hands the token directly to the waiter; count stays at 0.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_EQUAL_UINT32(0U, s_sem.count);
}

void test_sem_give_sets_schedule_pending_for_higher_priority_waiter(void)
{
    // task_a (priority 10) waits; idle (31) is the current task.
    // Give wakes task_a: 10 < 31, so is_schedule_pending is set.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER); // task_a BLOCKED

    // Switch current task to idle so task_a has higher priority than current.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch(); // current = 31, is_schedule_pending cleared

    (void)xRTOS_Sem_Give(&s_sem);

    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
}

// TESTS: xRTOS_Sem_Give_From_ISR //////////////////////////////////////////////

void test_sem_give_from_isr_sets_should_yield_true(void)
{
    // task_a (priority 10) waits; idle (31) is current.
    // ISR give wakes task_a: 10 < 31 -> should_yield == true.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch(); // current = 31 (idle)

    bool should_yield = false;
    (void)xRTOS_Sem_Give_From_ISR(&s_sem, &should_yield);

    TEST_ASSERT_TRUE(should_yield);
}

void test_sem_give_from_isr_wakes_blocked_task(void)
{
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER);

    bool should_yield = false;
    (void)xRTOS_Sem_Give_From_ISR(&s_sem, &should_yield);

    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_READY, s_task_a.state);
}

// TESTS: End-to-end timeout via tick ISR //////////////////////////////////////

void test_sem_take_timeout_via_isr(void)
{
    // task_a takes a locked sem with a 5-tick timeout. After 5 ISR ticks,
    // the task should time out with TIMEOUT block_status.
    (void)register_idle();
    (void)register_task(&s_task_a, s_stack_a, 10U);
    (void)xRTOS_Kernel_Start();
    (void)xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL);

    (void)xRTOS_Sem_Take(&s_sem, 5U); // blocks task_a (returns on host)

    // Fire 5 ticks - the 5th tick should expire the wait.
    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_TIMEOUT, s_task_a.block_status);
    TEST_ASSERT_EQUAL_INT(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_sem.wait_map, 10U));
}

// TEST RUNNER /////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_sem_init_sets_count);
    RUN_TEST(test_sem_init_sets_max_count);
    RUN_TEST(test_sem_init_clears_wait_map);
    RUN_TEST(test_sem_init_rejects_null);
    RUN_TEST(test_sem_init_rejects_zero_max_count);
    RUN_TEST(test_sem_init_rejects_initial_exceeds_max);
    RUN_TEST(test_sem_init_binary_available);

    RUN_TEST(test_sem_take_returns_ok_when_available);
    RUN_TEST(test_sem_take_decrements_count);
    RUN_TEST(test_sem_take_returns_would_block_no_wait_when_empty);
    RUN_TEST(test_sem_take_blocks_task);
    RUN_TEST(test_sem_take_sets_wait_map_bit);
    RUN_TEST(test_sem_take_uses_task_id_for_wait_map_bit);
    RUN_TEST(test_sem_take_arms_timeout_map_finite);
    RUN_TEST(test_sem_take_does_not_arm_timeout_map_wait_forever);
    RUN_TEST(test_sem_take_resets_stale_block_status_when_blocking);

    RUN_TEST(test_sem_give_increments_count);
    RUN_TEST(test_sem_give_returns_ok);
    RUN_TEST(test_sem_give_returns_full_at_max);
    RUN_TEST(test_sem_give_wakes_blocked_task);
    RUN_TEST(test_sem_give_sets_block_status_ok);
    RUN_TEST(test_sem_give_clears_wait_map_bit);
    RUN_TEST(test_sem_give_does_not_increment_count_when_waking_waiter);
    RUN_TEST(test_sem_give_sets_schedule_pending_for_higher_priority_waiter);

    RUN_TEST(test_sem_give_from_isr_sets_should_yield_true);
    RUN_TEST(test_sem_give_from_isr_wakes_blocked_task);

    RUN_TEST(test_sem_take_timeout_via_isr);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
