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

// @file test_xrtos_task_set_priority.c
// @brief Host unit tests for xRTOS_Task_Set_Priority.
//
// Covers:
//   1. Argument validation.
//   2. Non-boosted READY task: base/effective update, ready-list movement,
//      task_by_priority registry maintenance.
//   3. Non-boosted RUNNING task: priority mirrors, preemption detection.
//   4. Non-boosted BLOCKED (non-mutex) task: effective update with no ready-list churn.
//   5. PI-boosted task: boost survives when new base is weaker; effective follows
//      new base when new base beats the boost (regression test for the is_boosted
//      guard-too-broad bug).
//   6. Task blocked on a mutex: waiter-list repositioning and PI propagation
//      to the mutex owner, including a 2-level chained PI case.
//   7. next_priority mirror for the pre-selected next task.
//   8. is_schedule_pending correctness for raise/lower scenarios.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_mutex.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_fake.h"
#include "../src/xrtos_private.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Mutex_Context_t s_mutex;
static xRTOS_Mutex_Context_t s_mutex2;

static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_a; // base priority 10 - low-priority owner
static uint32_t s_stack_a[64U];

static xRTOS_Task_Context_t s_task_b; // base priority 5  - medium-high waiter
static uint32_t s_stack_b[64U];

static xRTOS_Task_Context_t s_task_c; // base priority 7  - medium waiter
static uint32_t s_stack_c[64U];

static xRTOS_Task_Context_t s_task_d; // base priority 15 - lowest priority
static uint32_t s_stack_d[64U];

#define IDLE_PRIORITY xRTOS_IDLE_PRIORITY
#define IDLE_ID       xRTOS_IDLE_TASK_ID

#define TASK_A_ID       (2U)
#define TASK_A_PRIORITY (10U)

#define TASK_B_ID       (3U)
#define TASK_B_PRIORITY (5U)

#define TASK_C_ID       (4U)
#define TASK_C_PRIORITY (7U)

#define TASK_D_ID       (6U)
#define TASK_D_PRIORITY (15U)

// HELPERS /////////////////////////////////////////////////////////////////////

static void dummy_entry(void *arg)
{
    (void)arg;
}

static xRETURN_t register_task(xRTOS_Task_Context_t *ctx, uint32_t *stack, uint32_t id, uint32_t priority)
{
    xRTOS_Task_Config_t cfg = {id, priority, dummy_entry, NULL, stack, 64U, NULL};
    return xRTOS_Task_Create(ctx, &cfg);
}

static void simulate_task_running(xRTOS_Task_Context_t *ctx)
{
    xrtos_scheduler_ready_remove(&s_kernel, ctx->task_id);
    ctx->state = xRTOS_TASK_STATE_RUNNING;
    s_kernel.scheduler.current_task_id = ctx->task_id;
    s_kernel.scheduler.current_priority = ctx->effective_priority;
}

static void simulate_task_ready(xRTOS_Task_Context_t *ctx)
{
    ctx->state = xRTOS_TASK_STATE_READY;
    xrtos_scheduler_ready_add(&s_kernel, ctx->task_id);
}

static void simulate_task_blocked(xRTOS_Task_Context_t *ctx)
{
    xrtos_scheduler_ready_remove(&s_kernel, ctx->task_id);
    ctx->state = xRTOS_TASK_STATE_BLOCKED;
    xRTOS_Bitmap_Set(&s_kernel.scheduler.blocked_map, ctx->task_id);
}

// Kernel start with only task_a (prio 10) + idle. task_a becomes RUNNING.
static void kernel_start_a_running(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Set up: task_a owns s_mutex, task_b (prio 5) blocks on it -> task_a PI-boosted to 5.
// After call: task_a is RUNNING with base=10, effective=5.
static void setup_pi_boost_a_owns_b_waits(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER); // A acquires mutex

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER); // B blocks -> A boosted to 5

    // Sanity: A's effective should now equal B's priority.
    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);

    simulate_task_running(&s_task_a);
}

// Set up: task_a (10) owns s_mutex; task_b (5) and task_c (7) both block on it.
// Wait list: task_b (5) -> task_c (7). task_a boosted to 5. task_a is RUNNING.
static void setup_two_waiters_a_owns(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task(&s_task_c, s_stack_c, TASK_C_ID, TASK_C_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_ready(&s_task_c);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    simulate_task_running(&s_task_c);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER); // C blocks at priority 7

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER); // B blocks at priority 5 -> A boosted to 5

    simulate_task_running(&s_task_a);
}

// Set up chained PI: task_d (15) owns s_mutex2; task_a (10) owns s_mutex and
// blocks on s_mutex2; task_b (5) blocks on s_mutex.
// After: a.effective = 5, d.effective = 5 (chain propagated).
static void setup_chained_pi(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task(&s_task_d, s_stack_d, TASK_D_ID, TASK_D_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_a);
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_d);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER); // D owns mutex2

    simulate_task_ready(&s_task_d);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);  // A owns mutex
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER); // A blocks on mutex2 -> boosts D

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER); // B blocks on mutex -> boosts A -> boosts D
}

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Mutex_Init(&s_mutex, NULL);
    (void)xRTOS_Mutex_Init(&s_mutex2, NULL);
}

void tearDown(void)
{
}

// ARGUMENT VALIDATION /////////////////////////////////////////////////////////

void test_set_priority_task_id_out_of_range_returns_invalid_arg(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Task_Set_Priority(xRTOS_MAX_TASKS, 5U));
}

void test_set_priority_new_priority_idle_returns_invalid_arg(void)
{
    kernel_start_a_running();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Task_Set_Priority(TASK_A_ID, xRTOS_IDLE_PRIORITY));
}

void test_set_priority_new_priority_out_of_range_returns_invalid_arg(void)
{
    kernel_start_a_running();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Task_Set_Priority(TASK_A_ID, xRTOS_MAX_PRIORITIES));
}

void test_set_priority_null_task_in_table_returns_invalid_arg(void)
{
    // Valid task_id that was never created - NULL slot in task_table.
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Task_Set_Priority(5U, 3U));
}

void test_set_priority_same_priority_is_noop_returns_ok(void)
{
    kernel_start_a_running();
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Task_Set_Priority(TASK_A_ID, TASK_A_PRIORITY));
    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.effective_priority);
}

// NON-BOOSTED READY TASK //////////////////////////////////////////////////////

void test_set_priority_ready_raise_updates_base_and_effective(void)
{
    kernel_start_a_running();
    simulate_task_ready(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.effective_priority);
}

void test_set_priority_ready_raise_moves_to_new_ready_bucket(void)
{
    kernel_start_a_running();
    simulate_task_ready(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_PTR(&s_task_a, s_kernel.scheduler.ready_head[3U]);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_priority_map, TASK_A_PRIORITY));
}

void test_set_priority_ready_lower_moves_to_new_ready_bucket(void)
{
    kernel_start_a_running();
    simulate_task_ready(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 20U);

    TEST_ASSERT_EQUAL_UINT32(20U, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_PTR(&s_task_a, s_kernel.scheduler.ready_head[20U]);
    TEST_ASSERT_NULL(s_kernel.scheduler.ready_head[TASK_A_PRIORITY]);
}

// task_by_priority registry ///////////////////////////////////////////////////

void test_set_priority_clears_old_slot_when_only_task_at_priority(void)
{
    kernel_start_a_running();
    simulate_task_ready(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_NULL(s_kernel.task_by_priority[TASK_A_PRIORITY]);
    TEST_ASSERT_EQUAL_PTR(&s_task_a, s_kernel.task_by_priority[3U]);
}

void test_set_priority_transfers_old_slot_to_sibling_at_same_base(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    // task_b registered at same priority as task_a (shared base priority).
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_A_PRIORITY);
    (void)xRTOS_Kernel_Start();
    simulate_task_ready(&s_task_b);
    simulate_task_ready(&s_task_a);

    // task_a was registered first -> it is the sentinel.
    TEST_ASSERT_EQUAL_PTR(&s_task_a, s_kernel.task_by_priority[TASK_A_PRIORITY]);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    // Sentinel must transfer to the sibling that remains at TASK_A_PRIORITY.
    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_kernel.task_by_priority[TASK_A_PRIORITY]);
    TEST_ASSERT_EQUAL_PTR(&s_task_a, s_kernel.task_by_priority[3U]);
}

void test_set_priority_does_not_overwrite_occupied_new_slot(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    simulate_task_ready(&s_task_b);
    simulate_task_ready(&s_task_a);

    // task_b already occupies slot TASK_B_PRIORITY. Moving task_a there must
    // not overwrite the sentinel.
    (void)xRTOS_Task_Set_Priority(TASK_A_ID, TASK_B_PRIORITY);

    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_kernel.task_by_priority[TASK_B_PRIORITY]);
}

// NON-BOOSTED RUNNING TASK ////////////////////////////////////////////////////

void test_set_priority_running_raise_updates_effective_and_current_priority(void)
{
    kernel_start_a_running();
    // Kernel_Start leaves task_a as RUNNING.

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel.scheduler.current_priority);
}

void test_set_priority_running_lower_below_ready_peer_sets_schedule_pending(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);

    // A runs at prio 10, B ready at prio 5. Lower A to prio 15 -> B (5) outranks A.
    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 15U);

    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
    TEST_ASSERT_EQUAL_UINT32(15U, s_kernel.scheduler.current_priority);
}

void test_set_priority_running_raise_above_peers_no_schedule_pending(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);

    // A at prio 10, B ready at prio 5. B outranks A (5 < 10).
    // Raise A to prio 3 - now A (3) outranks B (5) -> no pending switch needed.
    s_kernel.scheduler.is_schedule_pending = false;
    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_FALSE(s_kernel.scheduler.is_schedule_pending);
}

// NON-BOOSTED BLOCKED (non-mutex) TASK ////////////////////////////////////////

void test_set_priority_blocked_task_updates_effective_without_ready_list_churn(void)
{
    kernel_start_a_running();
    simulate_task_blocked(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_map, TASK_A_ID));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_priority_map, 3U));
}

void test_set_priority_blocked_task_lower_updates_effective(void)
{
    kernel_start_a_running();
    simulate_task_blocked(&s_task_a);

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 20U);

    TEST_ASSERT_EQUAL_UINT32(20U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(20U, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

// PI-BOOSTED TASK /////////////////////////////////////////////////////////////

// new base is WEAKER (numerically larger) than boost -> boost survives, effective unchanged.
void test_set_priority_boosted_task_boost_survives_when_new_base_weaker(void)
{
    setup_pi_boost_a_owns_b_waits();
    // A: base=10, effective=5. Raise base to 7 (7 > 5 -> boost still wins).

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 7U);

    TEST_ASSERT_EQUAL_UINT32(7U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority); // boost at 5 unchanged
}

// new base BEATS the boost (numerically smaller) -> effective follows new base.
// This is the regression test for the is_boosted-guard-too-broad fix.
void test_set_priority_boosted_task_effective_updated_when_new_base_beats_boost(void)
{
    setup_pi_boost_a_owns_b_waits();
    // A: base=10, effective=5 (boosted). Raise base to 3 - new base beats boost.

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(3U, s_task_a.effective_priority); // new base wins, NOT stale 5
}

// current_priority mirror: only updated when effective changes.
void test_set_priority_boosted_running_boost_survives_current_priority_unchanged(void)
{
    setup_pi_boost_a_owns_b_waits();
    s_kernel.scheduler.current_priority = s_task_a.effective_priority; // set to 5

    // new base 8 -> 8 > 5 -> boost still active, effective stays 5.
    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 8U);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_kernel.scheduler.current_priority); // still 5
}

void test_set_priority_boosted_running_current_priority_updated_when_base_wins(void)
{
    setup_pi_boost_a_owns_b_waits();
    s_kernel.scheduler.current_priority = s_task_a.effective_priority; // set to 5

    // new base 3 -> 3 < 5 -> new base wins, effective becomes 3.
    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel.scheduler.current_priority);
}

// BLOCKED-ON-MUTEX: WAITER LIST REPOSITIONING /////////////////////////////////

void test_set_priority_blocked_waiter_raise_repositions_to_list_head(void)
{
    setup_two_waiters_a_owns();
    // Wait list: task_b (5) -> task_c (7). Raise task_c to prio 2.
    // Expected after: task_c (2) -> task_b (5).

    (void)xRTOS_Task_Set_Priority(TASK_C_ID, 2U);

    TEST_ASSERT_EQUAL_UINT32(2U, s_task_c.effective_priority);
    TEST_ASSERT_EQUAL_PTR(&s_task_c, s_mutex.wait_head);
    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_mutex.wait_tail);
    TEST_ASSERT_NULL(s_task_c.mutex_wait_prev);
    TEST_ASSERT_EQUAL_PTR(&s_task_c, s_task_b.mutex_wait_prev);
}

void test_set_priority_blocked_waiter_lower_repositions_to_list_tail(void)
{
    setup_two_waiters_a_owns();
    // Wait list: task_b (5) -> task_c (7). Lower task_b to prio 9.
    // Expected after: task_c (7) -> task_b (9).

    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 9U);

    TEST_ASSERT_EQUAL_UINT32(9U, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_PTR(&s_task_c, s_mutex.wait_head);
    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_mutex.wait_tail);
}

// BLOCKED-ON-MUTEX: PI PROPAGATION TO OWNER ///////////////////////////////////

void test_set_priority_blocked_waiter_raise_propagates_pi_to_owner(void)
{
    setup_pi_boost_a_owns_b_waits();
    // A boosted to 5 by B. Raise B to priority 2 -> A should now be boosted to 2.

    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 2U);

    TEST_ASSERT_EQUAL_UINT32(2U, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(2U, s_task_a.effective_priority);
}

void test_set_priority_blocked_waiter_lower_updates_pi_to_remaining_highest(void)
{
    setup_two_waiters_a_owns();
    // A boosted to 5 by B (head of wait list). Lower B to priority 9.
    // C (prio 7) now becomes the highest waiter -> A should be re-boosted to 7.

    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 9U);

    TEST_ASSERT_EQUAL_UINT32(TASK_C_PRIORITY, s_task_a.effective_priority); // 7
}

void test_set_priority_blocked_waiter_lower_to_sole_waiter_updates_pi(void)
{
    setup_pi_boost_a_owns_b_waits();
    // A boosted to 5 by B. Lower B to priority 8 -> A re-boosted to 8.

    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 8U);

    TEST_ASSERT_EQUAL_UINT32(8U, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(8U, s_task_a.effective_priority);
}

// CHAINED PI: 2-LEVEL PROPAGATION /////////////////////////////////////////////

// Chain: B blocks on s_mutex owned by A, A blocks on s_mutex2 owned by D.
// After B's priority is raised, both A and D should inherit the new priority.
void test_set_priority_chained_pi_propagates_through_two_levels(void)
{
    setup_chained_pi();
    // A: base=10, effective=5 (from B). D: base=15, effective=5 (from A via chain).
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_d.effective_priority);

    // Raise B from prio 5 to prio 2.
    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 2U);

    TEST_ASSERT_EQUAL_UINT32(2U, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(2U, s_task_a.effective_priority); // propagated one level
    TEST_ASSERT_EQUAL_UINT32(2U, s_task_d.effective_priority); // propagated two levels
}

void test_set_priority_chained_pi_lower_propagates_through_two_levels(void)
{
    setup_chained_pi();
    // Lower B from prio 5 to prio 12.
    // A's base is 10, so it reverts to 10 (not boosted to 12, as 10 is higher priority than 12).
    // D's base is 15, and is boosted by A's effective priority (10) -> D becomes 10.
    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 12U);

    TEST_ASSERT_EQUAL_UINT32(12U, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(10U, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(10U, s_task_d.effective_priority);
}

// next_priority MIRROR ////////////////////////////////////////////////////////

void test_set_priority_mirrors_next_priority_for_preselected_task(void)
{
    kernel_start_a_running();
    simulate_task_ready(&s_task_a);
    s_kernel.scheduler.next_task_id = TASK_A_ID;
    s_kernel.scheduler.next_priority = TASK_A_PRIORITY;

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U);

    TEST_ASSERT_EQUAL_UINT32(3U, s_kernel.scheduler.next_priority);
}

void test_set_priority_does_not_change_next_priority_for_different_task(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    s_kernel.scheduler.next_task_id = TASK_B_ID;
    s_kernel.scheduler.next_priority = TASK_B_PRIORITY;

    (void)xRTOS_Task_Set_Priority(TASK_A_ID, 3U); // changing A, not B

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_kernel.scheduler.next_priority); // unchanged
}

// PREEMPTION DETECTION ////////////////////////////////////////////////////////

void test_set_priority_raise_ready_task_above_running_sets_schedule_pending(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    // Arrange: A running at prio 3 (simulate), B ready at prio 10.
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    s_task_a.effective_priority = 3U;
    s_kernel.scheduler.current_priority = 3U;
    s_kernel.scheduler.is_schedule_pending = false;

    // Raise B from prio 10 to prio 2 - B (2) now outranks A (3).
    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 2U);

    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
}

void test_set_priority_raise_ready_task_but_still_below_running_no_pending(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_ID, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
    // A running at prio 2, B ready at prio 10.
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    s_task_a.effective_priority = 2U;
    s_kernel.scheduler.current_priority = 2U;
    s_kernel.scheduler.is_schedule_pending = false;

    // Raise B from prio 10 to prio 5 - A (2) still outranks B (5).
    (void)xRTOS_Task_Set_Priority(TASK_B_ID, 5U);

    TEST_ASSERT_FALSE(s_kernel.scheduler.is_schedule_pending);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Argument validation
    RUN_TEST(test_set_priority_task_id_out_of_range_returns_invalid_arg);
    RUN_TEST(test_set_priority_new_priority_idle_returns_invalid_arg);
    RUN_TEST(test_set_priority_new_priority_out_of_range_returns_invalid_arg);
    RUN_TEST(test_set_priority_null_task_in_table_returns_invalid_arg);
    RUN_TEST(test_set_priority_same_priority_is_noop_returns_ok);

    // Non-boosted READY task
    RUN_TEST(test_set_priority_ready_raise_updates_base_and_effective);
    RUN_TEST(test_set_priority_ready_raise_moves_to_new_ready_bucket);
    RUN_TEST(test_set_priority_ready_lower_moves_to_new_ready_bucket);
    RUN_TEST(test_set_priority_clears_old_slot_when_only_task_at_priority);
    RUN_TEST(test_set_priority_transfers_old_slot_to_sibling_at_same_base);
    RUN_TEST(test_set_priority_does_not_overwrite_occupied_new_slot);

    // Non-boosted RUNNING task
    RUN_TEST(test_set_priority_running_raise_updates_effective_and_current_priority);
    RUN_TEST(test_set_priority_running_lower_below_ready_peer_sets_schedule_pending);
    RUN_TEST(test_set_priority_running_raise_above_peers_no_schedule_pending);

    // Non-boosted BLOCKED (non-mutex) task
    RUN_TEST(test_set_priority_blocked_task_updates_effective_without_ready_list_churn);
    RUN_TEST(test_set_priority_blocked_task_lower_updates_effective);

    // PI-boosted task (includes regression for is_boosted-guard-too-broad bug)
    RUN_TEST(test_set_priority_boosted_task_boost_survives_when_new_base_weaker);
    RUN_TEST(test_set_priority_boosted_task_effective_updated_when_new_base_beats_boost);
    RUN_TEST(test_set_priority_boosted_running_boost_survives_current_priority_unchanged);
    RUN_TEST(test_set_priority_boosted_running_current_priority_updated_when_base_wins);

    // Blocked-on-mutex: waiter-list repositioning
    RUN_TEST(test_set_priority_blocked_waiter_raise_repositions_to_list_head);
    RUN_TEST(test_set_priority_blocked_waiter_lower_repositions_to_list_tail);

    // Blocked-on-mutex: PI propagation to owner
    RUN_TEST(test_set_priority_blocked_waiter_raise_propagates_pi_to_owner);
    RUN_TEST(test_set_priority_blocked_waiter_lower_updates_pi_to_remaining_highest);
    RUN_TEST(test_set_priority_blocked_waiter_lower_to_sole_waiter_updates_pi);

    // Chained PI propagation
    RUN_TEST(test_set_priority_chained_pi_propagates_through_two_levels);
    RUN_TEST(test_set_priority_chained_pi_lower_propagates_through_two_levels);

    // next_priority mirror
    RUN_TEST(test_set_priority_mirrors_next_priority_for_preselected_task);
    RUN_TEST(test_set_priority_does_not_change_next_priority_for_different_task);

    // Preemption detection
    RUN_TEST(test_set_priority_raise_ready_task_above_running_sets_schedule_pending);
    RUN_TEST(test_set_priority_raise_ready_task_but_still_below_running_no_pending);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
