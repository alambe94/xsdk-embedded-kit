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

// @file test_xrtos_mutex.c
// @brief Host tests for xRTOS mutex (Phase 13, priority inheritance deferred).
//
// Tests cover:
//   1. xRTOS_Mutex_Init   - field initialization and argument validation.
//   2. xRTOS_Mutex_Lock   - fast path (unlocked), WOULD_BLOCK on NO_WAIT,
//      blocking setup (wait_map bit, blocked state, wait_map_ptr).
//   3. xRTOS_Mutex_Unlock - non-owner rejection, no-waiter release,
//      waiter wake path (ownership handoff, schedule pending).
//
// Host simulation note:
//   xRTOS_Scheduler_Block_Current does not perform a real context switch.
//   Lock on a held mutex transitions the caller to BLOCKED and returns immediately
//   with the default block_status (OK).  Tests verify intermediate bitmap/state
//   fields after Lock returns.  Wake-path tests call Unlock immediately after.
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

static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_a;
static uint32_t s_stack_a[64U];

static xRTOS_Task_Context_t s_task_b;
static uint32_t s_stack_b[64U];

static xRTOS_Task_Context_t s_task_c;
static uint32_t s_stack_c[64U];

static xRTOS_Task_Context_t s_task_d;
static uint32_t s_stack_d[64U];

static xRTOS_Mutex_Context_t s_mutex;
static xRTOS_Mutex_Context_t s_mutex2;

#define IDLE_PRIORITY   xRTOS_IDLE_PRIORITY
#define IDLE_ID         xRTOS_IDLE_TASK_ID
#define TASK_A_PRIORITY (10U)
#define TASK_B_PRIORITY (5U)
#define TASK_C_PRIORITY (7U)
#define TASK_D_PRIORITY (15U)
#define TASK_A_ID       (2U)
#define TASK_B_ID       (3U)
#define TASK_C_ID       (4U)
#define TASK_D_ID       (6U)

// HELPERS /////////////////////////////////////////////////////////////////////

static void dummy_entry(void *arg)
{
    (void)arg;
}

static xRETURN_t register_task(xRTOS_Task_Context_t *ctx, uint32_t *stack, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = (priority == IDLE_PRIORITY) ? IDLE_ID : priority;
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

static void simulate_task_ready(xRTOS_Task_Context_t *ctx)
{
    ctx->state = xRTOS_TASK_STATE_READY;
    xrtos_scheduler_ready_add(&s_kernel, ctx->task_id);
}

static void simulate_task_running(xRTOS_Task_Context_t *ctx)
{
    xrtos_scheduler_ready_remove(&s_kernel, ctx->task_id);
    ctx->state = xRTOS_TASK_STATE_RUNNING;
    s_kernel.scheduler.current_task_id = ctx->task_id;
    s_kernel.scheduler.current_priority = ctx->effective_priority;
}

// Register idle + task_a, start kernel so task_a is RUNNING.
static void kernel_start_with_task_a(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Register idle + task_a + task_b, start kernel so task_b is RUNNING (prio 5).
// Then simulate task_a running and locking the mutex, then task_b contending.
// After this call: task_b is BLOCKED on s_mutex.wait_map.
static void simulate_contention(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    // Hand control to task_a so it can lock.
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);

    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    // Now task_b runs and contends.
    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);

    // task_b blocks (host returns immediately).
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
}

static void simulate_contention_with_timeout(uint32_t timeout_ticks)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, timeout_ticks);
}

static void simulate_task_a_owns_current_b(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);
}

static void simulate_contention_with_distinct_ids(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    // Task A owns the mutex first even though task B has the higher priority.
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);

    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    // Task B contends and blocks. This should boost task A.
    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);

    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
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

// INIT TESTS //////////////////////////////////////////////////////////////////

void test_mutex_init_null_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Mutex_Init(NULL, NULL));
}

void test_mutex_init_owner_invalid(void)
{
    TEST_ASSERT_EQUAL(xRTOS_INVALID_TASK_ID, s_mutex.owner_task_id);
}

void test_mutex_init_wait_map_empty(void)
{
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_mutex.wait_map));
}

// LOCK - FAST PATH ////////////////////////////////////////////////////////////

void test_mutex_lock_null_ctx(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Mutex_Lock(NULL, xRTOS_WAIT_FOREVER));
}

void test_mutex_lock_fast_path_ok(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER));
}

void test_mutex_lock_fast_path_records_owner(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(s_task_a.task_id, s_mutex.owner_task_id);
}

void test_mutex_lock_fast_path_wait_map_empty(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_mutex.wait_map));
}

// LOCK - RECURSIVE ////////////////////////////////////////////////////////////

void test_mutex_lock_recursive_rejected(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER));
}

void test_mutex_lock_recursive_does_not_block_owner(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_RUNNING, s_task_a.state);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.blocked_map, s_task_a.task_id));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_mutex.wait_map));
}

// LOCK - NO_WAIT //////////////////////////////////////////////////////////////

void test_mutex_lock_no_wait_would_block(void)
{
    simulate_task_a_owns_current_b();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, xRTOS_Mutex_Lock(&s_mutex, xRTOS_NO_WAIT));
}

void test_mutex_lock_no_wait_wait_map_unchanged(void)
{
    simulate_task_a_owns_current_b();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_NO_WAIT);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_mutex.wait_map));
}

// LOCK - BLOCKING SETUP ///////////////////////////////////////////////////////

void test_mutex_lock_blocking_contender_in_wait_map(void)
{
    simulate_contention();
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_PRIORITY));
}

void test_mutex_lock_blocking_contender_state_blocked(void)
{
    simulate_contention();
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_b.state);
}

void test_mutex_lock_blocking_contender_wait_map_ptr(void)
{
    simulate_contention();
    TEST_ASSERT_EQUAL_PTR(&s_mutex.wait_map, s_task_b.wait_map_ptr);
}

void test_mutex_lock_blocking_contender_in_priority_wait_list(void)
{
    simulate_contention();
    TEST_ASSERT_EQUAL_PTR(&s_mutex, s_task_b.mutex_waiting_on);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_b.effective_priority);
    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_mutex.wait_head);
    TEST_ASSERT_EQUAL_PTR(&s_task_b, s_mutex.wait_tail);
}

void test_mutex_lock_blocking_timeout_map_armed_for_finite_wait(void)
{
    simulate_contention_with_timeout(10U);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_B_PRIORITY));
}

void test_mutex_lock_blocking_timeout_map_not_armed_for_wait_forever(void)
{
    simulate_contention(); // task_b blocked with WAIT_FOREVER
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_B_PRIORITY));
}

// UNLOCK - NON-OWNER REJECTION ////////////////////////////////////////////////

void test_mutex_unlock_null_ctx(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Mutex_Unlock(NULL));
}

void test_mutex_unlock_unlocked_mutex_rejected(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Mutex_Unlock(&s_mutex));
}

void test_mutex_unlock_invalid_owner_id_rejected(void)
{
    kernel_start_with_task_a();
    s_mutex.owner_task_id = xRTOS_INVALID_TASK_ID;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Mutex_Unlock(&s_mutex));
}

void test_mutex_unlock_non_owner_rejected(void)
{
    simulate_contention();
    // current is task_b (prio 5); mutex is owned by task_a.
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Mutex_Unlock(&s_mutex));
}

// UNLOCK - NO-WAITER RELEASE //////////////////////////////////////////////////

void test_mutex_unlock_no_waiters_clears_owner(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Unlock(&s_mutex);
    TEST_ASSERT_EQUAL(xRTOS_INVALID_TASK_ID, s_mutex.owner_task_id);
}

void test_mutex_unlock_no_waiters_returns_ok(void)
{
    kernel_start_with_task_a();
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Mutex_Unlock(&s_mutex));
}

// UNLOCK - WAITER WAKE PATH ///////////////////////////////////////////////////

// After contention: switch current back to task_a so it can unlock.
static void do_owner_unlock(void)
{
    simulate_contention();
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Unlock(&s_mutex);
}

void test_mutex_unlock_waiter_transitions_to_ready(void)
{
    do_owner_unlock();
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
}

void test_mutex_unlock_waiter_block_status_ok(void)
{
    do_owner_unlock();
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, s_task_b.block_status);
}

void test_mutex_unlock_waiter_in_ready_map(void)
{
    do_owner_unlock();
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_map, s_task_b.task_id));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_priority_map, TASK_B_PRIORITY));
}

void test_mutex_unlock_ownership_transferred(void)
{
    do_owner_unlock();
    TEST_ASSERT_EQUAL(s_task_b.task_id, s_mutex.owner_task_id);
}

void test_mutex_unlock_wait_map_cleared(void)
{
    do_owner_unlock();
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_mutex.wait_map));
}

void test_mutex_unlock_handoff_clears_waiter_links(void)
{
    do_owner_unlock();
    TEST_ASSERT_NULL(s_task_b.mutex_waiting_on);
    TEST_ASSERT_NULL(s_task_b.mutex_wait_prev);
    TEST_ASSERT_NULL(s_task_b.mutex_wait_next);
    TEST_ASSERT_NULL(s_mutex.wait_head);
    TEST_ASSERT_NULL(s_mutex.wait_tail);
}

void test_mutex_unlock_schedule_pending_when_waiter_outranks(void)
{
    // task_b (prio 5) woken while current is task_a (prio 10) -> 5 < 10 -> pending.
    do_owner_unlock();
    TEST_ASSERT_TRUE(s_kernel.scheduler.is_schedule_pending);
}

void test_mutex_lock_pi_boosts_owner_effective_priority(void)
{
    simulate_contention_with_distinct_ids();

    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.base_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);
}

void test_mutex_lock_wait_map_uses_task_id_not_priority(void)
{
    simulate_contention_with_distinct_ids();

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_ID));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_PRIORITY));
}

void test_mutex_unlock_restores_owner_base_priority_after_pi(void)
{
    simulate_contention_with_distinct_ids();

    simulate_task_running(&s_task_a);

    (void)xRTOS_Mutex_Unlock(&s_mutex);

    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.effective_priority);
}

void test_mutex_unlock_handoff_uses_waiter_task_id_after_pi(void)
{
    simulate_contention_with_distinct_ids();

    simulate_task_running(&s_task_a);

    (void)xRTOS_Mutex_Unlock(&s_mutex);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_ID, s_mutex.owner_task_id);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_map, TASK_B_ID));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_priority_map, TASK_B_PRIORITY));
}

void test_mutex_timeout_restores_owner_base_priority_after_pi(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, 5U);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_ID));
    TEST_ASSERT_NULL(s_mutex.wait_head);
    TEST_ASSERT_NULL(s_mutex.wait_tail);
    TEST_ASSERT_NULL(s_task_b.mutex_waiting_on);
    TEST_ASSERT_NULL(s_task_b.block_cleanup);
    TEST_ASSERT_NULL(s_task_b.block_cleanup_arg);
}

void test_mutex_timeout_restores_owner_to_remaining_waiter_priority(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task_with_id(&s_task_c, s_stack_c, TASK_C_ID, TASK_C_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_ready(&s_task_c);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_c);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(TASK_C_PRIORITY, s_task_a.effective_priority);

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, 5U);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL_UINT32(TASK_C_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_ID));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_C_ID));
    TEST_ASSERT_EQUAL_PTR(&s_task_c, s_mutex.wait_head);
    TEST_ASSERT_EQUAL_PTR(&s_task_c, s_mutex.wait_tail);
    TEST_ASSERT_NULL(s_task_b.block_cleanup);
    TEST_ASSERT_NULL(s_task_b.block_cleanup_arg);
}

void test_mutex_unlock_one_of_two_held_mutexes_keeps_remaining_inheritance(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task_with_id(&s_task_c, s_stack_c, TASK_C_ID, TASK_C_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_b);
    simulate_task_ready(&s_task_c);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_a);
    simulate_task_running(&s_task_c);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(TASK_C_PRIORITY, s_task_a.effective_priority);

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);

    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Unlock(&s_mutex2);

    TEST_ASSERT_EQUAL_UINT32(TASK_C_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_ID, s_mutex2.owner_task_id);
    TEST_ASSERT_EQUAL_UINT32(TASK_A_ID, s_mutex.owner_task_id);
}

void test_mutex_chained_pi_boosts_owner_blocked_on_second_mutex(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task_with_id(&s_task_d, s_stack_d, TASK_D_ID, TASK_D_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_a);
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_d);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_d);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_d.effective_priority);

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_d.effective_priority);
}

void test_mutex_chained_pi_timeout_restores_upstream_owner_priority(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, TASK_A_ID, TASK_A_PRIORITY);
    (void)register_task_with_id(&s_task_b, s_stack_b, TASK_B_ID, TASK_B_PRIORITY);
    (void)register_task_with_id(&s_task_d, s_stack_d, TASK_D_ID, TASK_D_PRIORITY);
    (void)xRTOS_Kernel_Start();

    simulate_task_ready(&s_task_a);
    simulate_task_ready(&s_task_b);
    simulate_task_running(&s_task_d);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    simulate_task_ready(&s_task_d);
    simulate_task_running(&s_task_a);
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);
    (void)xRTOS_Mutex_Lock(&s_mutex2, xRTOS_WAIT_FOREVER);

    simulate_task_running(&s_task_b);
    (void)xRTOS_Mutex_Lock(&s_mutex, 5U);

    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_B_PRIORITY, s_task_d.effective_priority);

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_a.effective_priority);
    TEST_ASSERT_EQUAL_UINT32(TASK_A_PRIORITY, s_task_d.effective_priority);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_mutex.wait_map, TASK_B_ID));
    TEST_ASSERT_NULL(s_mutex.wait_head);
    TEST_ASSERT_NULL(s_mutex.wait_tail);
    TEST_ASSERT_NULL(s_task_b.mutex_waiting_on);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Init
    RUN_TEST(test_mutex_init_null_ctx);
    RUN_TEST(test_mutex_init_owner_invalid);
    RUN_TEST(test_mutex_init_wait_map_empty);

    // Lock - fast path
    RUN_TEST(test_mutex_lock_null_ctx);
    RUN_TEST(test_mutex_lock_fast_path_ok);
    RUN_TEST(test_mutex_lock_fast_path_records_owner);
    RUN_TEST(test_mutex_lock_fast_path_wait_map_empty);
    RUN_TEST(test_mutex_lock_recursive_rejected);
    RUN_TEST(test_mutex_lock_recursive_does_not_block_owner);

    // Lock - NO_WAIT
    RUN_TEST(test_mutex_lock_no_wait_would_block);
    RUN_TEST(test_mutex_lock_no_wait_wait_map_unchanged);

    // Lock - blocking setup
    RUN_TEST(test_mutex_lock_blocking_contender_in_wait_map);
    RUN_TEST(test_mutex_lock_blocking_contender_state_blocked);
    RUN_TEST(test_mutex_lock_blocking_contender_wait_map_ptr);
    RUN_TEST(test_mutex_lock_blocking_contender_in_priority_wait_list);
    RUN_TEST(test_mutex_lock_blocking_timeout_map_armed_for_finite_wait);
    RUN_TEST(test_mutex_lock_blocking_timeout_map_not_armed_for_wait_forever);

    // Unlock - non-owner rejection
    RUN_TEST(test_mutex_unlock_null_ctx);
    RUN_TEST(test_mutex_unlock_unlocked_mutex_rejected);
    RUN_TEST(test_mutex_unlock_invalid_owner_id_rejected);
    RUN_TEST(test_mutex_unlock_non_owner_rejected);

    // Unlock - no-waiter release
    RUN_TEST(test_mutex_unlock_no_waiters_clears_owner);
    RUN_TEST(test_mutex_unlock_no_waiters_returns_ok);

    // Unlock - waiter wake path
    RUN_TEST(test_mutex_unlock_waiter_transitions_to_ready);
    RUN_TEST(test_mutex_unlock_waiter_block_status_ok);
    RUN_TEST(test_mutex_unlock_waiter_in_ready_map);
    RUN_TEST(test_mutex_unlock_ownership_transferred);
    RUN_TEST(test_mutex_unlock_wait_map_cleared);
    RUN_TEST(test_mutex_unlock_handoff_clears_waiter_links);
    RUN_TEST(test_mutex_unlock_schedule_pending_when_waiter_outranks);

    // Priority inheritance
    RUN_TEST(test_mutex_lock_pi_boosts_owner_effective_priority);
    RUN_TEST(test_mutex_lock_wait_map_uses_task_id_not_priority);
    RUN_TEST(test_mutex_unlock_restores_owner_base_priority_after_pi);
    RUN_TEST(test_mutex_unlock_handoff_uses_waiter_task_id_after_pi);
    RUN_TEST(test_mutex_timeout_restores_owner_base_priority_after_pi);
    RUN_TEST(test_mutex_timeout_restores_owner_to_remaining_waiter_priority);
    RUN_TEST(test_mutex_unlock_one_of_two_held_mutexes_keeps_remaining_inheritance);
    RUN_TEST(test_mutex_chained_pi_boosts_owner_blocked_on_second_mutex);
    RUN_TEST(test_mutex_chained_pi_timeout_restores_upstream_owner_priority);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
