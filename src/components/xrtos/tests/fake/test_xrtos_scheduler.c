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

// @file test_xrtos_scheduler.c
// @brief Host tests for the xRTOS bitmap scheduler (Phase 5).
//
// All tests run on the fake port which provides no-op implementations of
// context-switch primitives. Each test re-initialises the kernel via setUp so
// that global state does not bleed between tests.
//
// Most tests use task_id == priority for compact setup. Dedicated tests below
// cover the stable task_id / scheduling-priority split.
//

#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel_ctx;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_ctx[4];
static uint32_t s_stack[4][64U];

static void dummy_entry(void *arg)
{
    (void)arg;
}

// Creates and registers the mandatory idle task.
static xRETURN_t register_idle_task(void)
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

// Creates and registers a non-idle task. slot is 0..3, selects s_task_ctx and s_stack.
static xRETURN_t register_task(uint32_t slot, uint32_t task_id, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = task_id;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_stack[slot];
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return xRTOS_Task_Create(&s_task_ctx[slot], &cfg);
}

// Starts the scheduler (idle task required) and transitions current_priority to
// first_priority without calling port start (fake port returns immediately).
static xRETURN_t start_scheduler(void)
{
    return xRTOS_Kernel_Start();
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel_ctx, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: xRTOS_Scheduler_Select_Next //////////////////////////////////////////

void test_scheduler_selects_highest_priority_ready_task(void)
{
    // Register tasks at priorities 5, 10, 15 and idle.
    (void)register_task(0U, 5U, 5U);
    (void)register_task(1U, 10U, 10U);
    (void)register_task(2U, 15U, 15U);
    (void)register_idle_task();

    xRETURN_t ret = xRTOS_Scheduler_Select_Next();

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(5U, s_kernel_ctx.scheduler.next_priority);
}

void test_scheduler_select_next_returns_task_id_for_highest_priority_task(void)
{
    (void)register_task(0U, 9U, 3U);
    (void)register_task(1U, 4U, 8U);
    (void)register_idle_task();

    xRETURN_t ret = xRTOS_Scheduler_Select_Next();

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(9U, s_kernel_ctx.scheduler.next_task_id);
    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel_ctx.scheduler.next_priority);
}

void test_scheduler_falls_back_to_idle_when_no_user_task_ready(void)
{
    // Only the idle task is registered.
    (void)register_idle_task();

    xRETURN_t ret = xRTOS_Scheduler_Select_Next();

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_IDLE_PRIORITY, s_kernel_ctx.scheduler.next_priority);
}

void test_scheduler_task_create_sets_ready_priority_map(void)
{
    (void)register_task(0U, 9U, 3U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 3U));
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[3U]);
}

void test_scheduler_start_removes_running_task_from_ready_list(void)
{
    (void)register_task(0U, 9U, 3U);
    (void)register_idle_task();

    (void)start_scheduler();

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 3U));
    TEST_ASSERT_NULL(s_kernel_ctx.scheduler.ready_head[3U]);
    TEST_ASSERT_NULL(s_task_ctx[0].ready_prev);
    TEST_ASSERT_NULL(s_task_ctx[0].ready_next);
}

// TESTS: xRTOS_Scheduler_Block_Current ////////////////////////////////////////

void test_scheduler_block_clears_ready_bit(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler(); // current_priority = 2

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 2U));
}

void test_scheduler_block_sets_blocked_bit(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.blocked_map, 2U));
}

void test_scheduler_block_current_uses_task_id_not_priority(void)
{
    (void)register_task(0U, 9U, 3U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.blocked_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.blocked_map, 3U));
    TEST_ASSERT_NULL(s_task_ctx[0].ready_prev);
    TEST_ASSERT_NULL(s_task_ctx[0].ready_next);
}

void test_scheduler_block_sets_task_state_to_blocked(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_ctx[0].state);
}

void test_scheduler_block_stores_wait_map_ptr(void)
{
    (void)register_task(0U, 4U, 4U);
    (void)register_idle_task();
    (void)start_scheduler();

    xRTOS_Bitmap_t fake_wait_map = {{0U}};
    (void)xRTOS_Scheduler_Block_Current(&fake_wait_map);

    TEST_ASSERT_EQUAL_PTR(&fake_wait_map, s_task_ctx[0].wait_map_ptr);
}

void test_scheduler_block_sets_schedule_pending(void)
{
    (void)register_task(0U, 1U, 1U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_TRUE(s_kernel_ctx.scheduler.is_schedule_pending);
}

// TESTS: xRTOS_Scheduler_Unblock //////////////////////////////////////////////

void test_scheduler_unblock_sets_ready_bit(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Unblock(2U, xRETURN_xRTOS_OK);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 2U));
}

void test_scheduler_unblock_uses_task_id_not_priority(void)
{
    (void)register_task(0U, 9U, 3U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Unblock(9U, xRETURN_xRTOS_OK);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 3U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.blocked_map, 9U));
}

void test_scheduler_unblock_clears_blocked_bit(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Unblock(2U, xRETURN_xRTOS_OK);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.blocked_map, 2U));
}

void test_scheduler_unblock_sets_block_status(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Unblock(2U, xRETURN_xERR_xRTOS_TIMEOUT);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_TIMEOUT, s_task_ctx[0].block_status);
}

void test_scheduler_unblock_sets_task_state_to_ready(void)
{
    (void)register_task(0U, 6U, 6U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Unblock(6U, xRETURN_xRTOS_OK);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_ctx[0].state);
}

void test_scheduler_unblock_clears_wait_map_ptr(void)
{
    (void)register_task(0U, 7U, 7U);
    (void)register_idle_task();
    (void)start_scheduler();

    xRTOS_Bitmap_t fake_wait_map = {{0U}};
    (void)xRTOS_Scheduler_Block_Current(&fake_wait_map);
    (void)xRTOS_Scheduler_Unblock(7U, xRETURN_xRTOS_OK);

    TEST_ASSERT_NULL(s_task_ctx[0].wait_map_ptr);
}

void test_scheduler_unblock_sets_schedule_pending_for_higher_priority(void)
{
    // current = 5 (running); unblock priority 2 (higher) -> should preempt.
    (void)register_task(0U, 2U, 2U);
    (void)register_task(1U, 5U, 5U);
    (void)register_idle_task();
    (void)start_scheduler(); // current_priority = 2

    // Manually move to priority 5 running to simulate a lower-priority task running.
    // Block priority 2 first, then switch to idle so priority 5 becomes current.
    (void)xRTOS_Scheduler_Block_Current(NULL); // blocks priority 2

    // Select next (priority 5) and commit the switch.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch(); // current_priority = 5

    // Now unblock priority 2 (higher priority than current 5).
    s_kernel_ctx.scheduler.is_schedule_pending = false;
    (void)xRTOS_Scheduler_Unblock(2U, xRETURN_xRTOS_OK);

    TEST_ASSERT_TRUE(s_kernel_ctx.scheduler.is_schedule_pending);
}

void test_scheduler_unblock_does_not_set_schedule_pending_for_lower_priority(void)
{
    // current = 2 (running); unblock priority 8 (lower) -> no preemption.
    (void)register_task(0U, 2U, 2U);
    (void)register_task(1U, 8U, 8U);
    (void)register_idle_task();
    (void)start_scheduler(); // current_priority = 2

    // Block priority 8 manually via bitmap (simulate it was blocked by something).
    xRTOS_Bitmap_Clear(&s_kernel_ctx.scheduler.ready_map, 8U);
    xRTOS_Bitmap_Set(&s_kernel_ctx.scheduler.blocked_map, 8U);
    s_task_ctx[1].state = xRTOS_TASK_STATE_BLOCKED;

    s_kernel_ctx.scheduler.is_schedule_pending = false;
    (void)xRTOS_Scheduler_Unblock(8U, xRETURN_xRTOS_OK);

    TEST_ASSERT_FALSE(s_kernel_ctx.scheduler.is_schedule_pending);
}

void test_scheduler_unblock_inserts_task_at_effective_priority(void)
{
    (void)register_task(0U, 9U, 9U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Set_Effective_Priority(9U, 2U);
    (void)xRTOS_Scheduler_Unblock(9U, xRETURN_xRTOS_OK);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 2U));
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[2U]);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 9U));
}

// TESTS: xRTOS_Scheduler_Switch ///////////////////////////////////////////////

void test_scheduler_switch_updates_current_priority(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_task(1U, 7U, 7U);
    (void)register_idle_task();
    (void)start_scheduler(); // current = 3

    // Block task 3, select task 7 as next, commit switch.
    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    TEST_ASSERT_EQUAL_UINT32(7U, s_kernel_ctx.scheduler.current_priority);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 7U));
}

void test_scheduler_switch_marks_next_task_running(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_task(1U, 8U, 8U);
    (void)register_idle_task();
    (void)start_scheduler();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    // After blocking task at 3 and switching to 8, task at 8 must be RUNNING.
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_RUNNING, s_task_ctx[1].state);
}

void test_scheduler_switch_marks_previous_running_task_ready(void)
{
    (void)register_task(0U, 1U, 1U);
    (void)register_task(1U, 9U, 9U);
    (void)register_idle_task();
    (void)start_scheduler(); // current = 1, state = RUNNING

    // Artificially set next task to priority 9 without blocking task 1, simulating
    // a cooperative yield where the current task stays READY.
    s_kernel_ctx.scheduler.next_task_id = 9U;
    s_kernel_ctx.scheduler.next_priority = 9U;
    xRTOS_Scheduler_Switch();

    // The task at priority 1 was RUNNING; Switch must have moved it back to READY.
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_ctx[0].state);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_map, 1U));
}

void test_scheduler_switch_clears_schedule_pending(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle_task();
    (void)start_scheduler();

    s_kernel_ctx.scheduler.next_priority = xRTOS_IDLE_PRIORITY;
    s_kernel_ctx.scheduler.is_schedule_pending = true;
    xRTOS_Scheduler_Switch();

    TEST_ASSERT_FALSE(s_kernel_ctx.scheduler.is_schedule_pending);
}

void test_scheduler_effective_priority_change_moves_ready_task_between_lists(void)
{
    (void)register_task(0U, 10U, 10U);

    (void)xRTOS_Scheduler_Set_Effective_Priority(10U, 2U);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 10U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 2U));
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[2U]);
}

void test_scheduler_equal_effective_priority_uses_ready_list_order(void)
{
    (void)register_task(0U, 5U, 5U);
    (void)register_task(1U, 10U, 10U);

    (void)xRTOS_Scheduler_Set_Effective_Priority(10U, 5U);
    (void)xRTOS_Scheduler_Select_Next();

    TEST_ASSERT_EQUAL_UINT32(5U, s_kernel_ctx.scheduler.next_task_id);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[5U]);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[1], s_kernel_ctx.scheduler.ready_tail[5U]);
}

// TESTS: Shared Base Priority (Cooperative Scheduling) ///////////////////////

// Two tasks at the same base priority must both appear in ready_head[P] as a
// FIFO: the first registered at the head, the second at the tail.
void test_scheduler_shared_base_priority_both_tasks_in_ready_list(void)
{
    (void)register_task(0U, 2U, 5U); // task_id=2, priority=5 -> first
    (void)register_task(1U, 3U, 5U); // task_id=3, priority=5 -> second

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel_ctx.scheduler.ready_priority_map, 5U));
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[5U]);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[1], s_kernel_ctx.scheduler.ready_tail[5U]);

    // task_by_priority tracks the FIRST registered task only.
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.task_by_priority[5U]);
}

// After a context switch the outgoing RUNNING task goes to the tail, so the
// peer task (previously at tail) becomes the new head.
void test_scheduler_switch_places_running_task_at_tail_of_ready_list(void)
{
    (void)register_task(0U, 2U, 5U);
    (void)register_task(1U, 3U, 5U);
    (void)register_idle_task();
    (void)start_scheduler();

    // Scheduler selected task_id=2 (head) as running; it left the ready list.
    TEST_ASSERT_EQUAL_UINT32(2U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[1], s_kernel_ctx.scheduler.ready_head[5U]);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[1], s_kernel_ctx.scheduler.ready_tail[5U]);

    // Simulate peer being selected as next, then switch.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    // Previous running task (task_id=2) is now at the tail; peer (task_id=3)
    // is running and no longer in the ready list.
    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[5U]);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_tail[5U]);
}

// With only one task at a priority, Select_Next picks that task; the ready
// list still has it in head position (no peer to rotate to).
void test_scheduler_single_task_at_priority_stays_head(void)
{
    (void)register_task(0U, 2U, 5U);
    (void)register_idle_task();
    (void)start_scheduler();

    // Only task_id=2 at priority 5; after it starts running the list is empty.
    TEST_ASSERT_EQUAL_UINT32(2U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_NULL(s_kernel_ctx.scheduler.ready_head[5U]);

    // Switch selects idle; outgoing task goes back to head of its priority.
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[5U]);
}

// Three tasks at the same priority cycle in registration order after each switch.
void test_scheduler_three_tasks_same_priority_cycle_in_fifo_order(void)
{
    static xRTOS_Task_Context_t extra_ctx;
    static uint32_t extra_stack[64U];
    xRTOS_Task_Config_t cfg = {6U, 5U, dummy_entry, NULL, extra_stack, 64U, NULL};
    (void)register_task(0U, 2U, 5U);
    (void)register_task(1U, 3U, 5U);
    (void)xRTOS_Task_Create(&extra_ctx, &cfg);
    (void)register_idle_task();
    (void)start_scheduler();

    // Scheduler started with task_id=2 running (head). Ready: [3, 6].
    TEST_ASSERT_EQUAL_UINT32(2U, s_kernel_ctx.scheduler.current_task_id);

    // Switch 1: task_id=3 runs; task_id=2 goes to tail. Ready: [6, 2].
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_EQUAL_PTR(&extra_ctx, s_kernel_ctx.scheduler.ready_head[5U]);

    // Switch 2: task_id=6 runs; task_id=3 goes to tail. Ready: [2, 3].
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(6U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[0], s_kernel_ctx.scheduler.ready_head[5U]);

    // Switch 3: task_id=2 runs; task_id=6 goes to tail. Ready: [3, 6].
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    TEST_ASSERT_EQUAL_UINT32(2U, s_kernel_ctx.scheduler.current_task_id);
    TEST_ASSERT_EQUAL_PTR(&s_task_ctx[1], s_kernel_ctx.scheduler.ready_head[5U]);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_scheduler_selects_highest_priority_ready_task);
    RUN_TEST(test_scheduler_select_next_returns_task_id_for_highest_priority_task);
    RUN_TEST(test_scheduler_falls_back_to_idle_when_no_user_task_ready);
    RUN_TEST(test_scheduler_task_create_sets_ready_priority_map);
    RUN_TEST(test_scheduler_start_removes_running_task_from_ready_list);

    RUN_TEST(test_scheduler_block_clears_ready_bit);
    RUN_TEST(test_scheduler_block_sets_blocked_bit);
    RUN_TEST(test_scheduler_block_current_uses_task_id_not_priority);
    RUN_TEST(test_scheduler_block_sets_task_state_to_blocked);
    RUN_TEST(test_scheduler_block_stores_wait_map_ptr);
    RUN_TEST(test_scheduler_block_sets_schedule_pending);

    RUN_TEST(test_scheduler_unblock_sets_ready_bit);
    RUN_TEST(test_scheduler_unblock_uses_task_id_not_priority);
    RUN_TEST(test_scheduler_unblock_clears_blocked_bit);
    RUN_TEST(test_scheduler_unblock_sets_block_status);
    RUN_TEST(test_scheduler_unblock_sets_task_state_to_ready);
    RUN_TEST(test_scheduler_unblock_clears_wait_map_ptr);
    RUN_TEST(test_scheduler_unblock_sets_schedule_pending_for_higher_priority);
    RUN_TEST(test_scheduler_unblock_does_not_set_schedule_pending_for_lower_priority);
    RUN_TEST(test_scheduler_unblock_inserts_task_at_effective_priority);

    RUN_TEST(test_scheduler_switch_updates_current_priority);
    RUN_TEST(test_scheduler_switch_marks_next_task_running);
    RUN_TEST(test_scheduler_switch_marks_previous_running_task_ready);
    RUN_TEST(test_scheduler_switch_clears_schedule_pending);
    RUN_TEST(test_scheduler_effective_priority_change_moves_ready_task_between_lists);
    RUN_TEST(test_scheduler_equal_effective_priority_uses_ready_list_order);

    // Shared base priority / cooperative scheduling
    RUN_TEST(test_scheduler_shared_base_priority_both_tasks_in_ready_list);
    RUN_TEST(test_scheduler_switch_places_running_task_at_tail_of_ready_list);
    RUN_TEST(test_scheduler_single_task_at_priority_stays_head);
    RUN_TEST(test_scheduler_three_tasks_same_priority_cycle_in_fifo_order);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
