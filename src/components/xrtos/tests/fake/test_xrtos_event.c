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

// @file test_xrtos_event.c
// @brief Host tests for xRTOS event flags (Phase 14).
//
// Tests cover:
//   1. xRTOS_Event_Init   - zeroing and argument validation.
//   2. xRTOS_Event_Clear  - bit-clearing behaviour.
//   3. xRTOS_Event_Wait   - fast path (WAIT_ANY / WAIT_ALL / CLEAR_ON_EXIT),
//      WOULD_BLOCK on NO_WAIT, blocking setup (wait_map, state, masks, options,
//      timeout_map).
//   4. xRTOS_Event_Set    - flag accumulation, unicast wake (WAIT_ANY / WAIT_ALL),
//      no-wake when condition unmet, multicast (both waiters satisfied),
//      partial-multicast (only one waiter satisfied), CLEAR_ON_EXIT via Set.
//   5. xRTOS_Event_Set_From_ISR - should_yield output.
//
// Host simulation note:
//   Block_Current returns immediately on host. Event_Wait with unsatisfied
//   condition blocks the task and returns immediately with default block_status
//   (OK). Tests verify the intermediate state (BLOCKED, wait_map bit, etc.).
//   Wake-path tests call Set after the Wait returns and verify the resulting
//   READY state, flag values, and schedule-pending flag.
//

#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_event.h"
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

static xRTOS_Event_Context_t s_event;

#define IDLE_PRIORITY   xRTOS_IDLE_PRIORITY
#define IDLE_ID         xRTOS_IDLE_TASK_ID
#define TASK_A_PRIORITY (10U)
#define TASK_B_PRIORITY (5U)
// register_task sets task_id == priority, so these aliases exist to distinguish
// which role the constant fills at each use site.
#define TASK_A_ID TASK_A_PRIORITY
#define TASK_B_ID TASK_B_PRIORITY

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

static void simulate_current_task(xRTOS_Task_Context_t *ctx)
{
    s_kernel.scheduler.current_task_id = ctx->task_id;
    s_kernel.scheduler.current_priority = ctx->effective_priority;
    ctx->state = xRTOS_TASK_STATE_RUNNING;
    xRTOS_Bitmap_Clear(&s_kernel.scheduler.ready_map, ctx->task_id);
}

// Register idle + task_a, start kernel (task_a RUNNING at prio 10).
static void kernel_start_with_task_a(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Event_Init(&s_event, NULL);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Register idle + task_a + task_b, start kernel (task_b RUNNING at prio 5).
static void kernel_start_with_task_b(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Event_Init(&s_event, NULL);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Block task_a on the event waiting for `flags` with `options`.
static void block_task_a(uint32_t flags, uint32_t options)
{
    simulate_current_task(&s_task_a);
    (void)xRTOS_Event_Wait(&s_event, flags, options, xRTOS_WAIT_FOREVER, NULL);
}

// Block task_b on the event waiting for `flags` with `options`.
static void block_task_b(uint32_t flags, uint32_t options)
{
    simulate_current_task(&s_task_b);
    (void)xRTOS_Event_Wait(&s_event, flags, options, xRTOS_WAIT_FOREVER, NULL);
}

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Event_Init(&s_event, NULL);
}

void tearDown(void)
{
}

// INIT TESTS //////////////////////////////////////////////////////////////////

void test_event_init_null_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Event_Init(NULL, NULL));
}

void test_event_init_flags_zero(void)
{
    s_event.flags = 0xDEADBEEFU;
    (void)xRTOS_Event_Init(&s_event, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, s_event.flags);
}

void test_event_init_wait_map_empty(void)
{
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_event.wait_map));
}

// CLEAR TESTS /////////////////////////////////////////////////////////////////

void test_event_clear_null_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Event_Clear(NULL, 0xFFU));
}

void test_event_clear_clears_specified_bits(void)
{
    s_event.flags = 0x0FU;
    (void)xRTOS_Event_Clear(&s_event, 0x03U);
    TEST_ASSERT_EQUAL_UINT32(0x0CU, s_event.flags);
}

void test_event_clear_does_not_affect_other_bits(void)
{
    s_event.flags = 0xFFU;
    (void)xRTOS_Event_Clear(&s_event, 0x0FU);
    TEST_ASSERT_EQUAL_UINT32(0xF0U, s_event.flags);
}

void test_event_clear_returns_ok(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Event_Clear(&s_event, 0U));
}

// WAIT - FAST PATH ////////////////////////////////////////////////////////////

void test_event_wait_fast_path_any_satisfied(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0x02U;
    uint32_t got = 0U;
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x03U, xRTOS_EVENT_WAIT_ANY, xRTOS_NO_WAIT, &got);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0x02U, got);
}

void test_event_wait_fast_path_all_satisfied(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0x03U;
    uint32_t got = 0U;
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x03U, xRTOS_EVENT_WAIT_ALL, xRTOS_NO_WAIT, &got);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0x03U, got);
}

void test_event_wait_fast_path_all_not_satisfied(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0x01U; // only bit 0, need bits 0+1
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x03U, xRTOS_EVENT_WAIT_ALL, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, ret);
}

void test_event_wait_fast_path_clear_on_exit(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0x03U;
    (void)xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL_UINT32(0x02U, s_event.flags); // bit 0 cleared
}

void test_event_wait_fast_path_no_wait_would_block(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0U;
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, ret);
}

void test_event_wait_rejects_zero_wait_mask(void)
{
    kernel_start_with_task_a();
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0U, xRTOS_EVENT_WAIT_ANY, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_event_wait_rejects_missing_wait_mode(void)
{
    kernel_start_with_task_a();
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_CLEAR_ON_EXIT, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

void test_event_wait_rejects_both_wait_modes(void)
{
    kernel_start_with_task_a();
    uint32_t options = xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_WAIT_ALL;
    xRETURN_t ret = xRTOS_Event_Wait(&s_event, 0x01U, options, xRTOS_NO_WAIT, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
}

// WAIT - BLOCKING SETUP ///////////////////////////////////////////////////////

void test_event_wait_blocking_state_blocked(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_event_wait_blocking_in_wait_map(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_event.wait_map, TASK_A_ID));
}

void test_event_wait_blocking_wait_map_ptr_set(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    TEST_ASSERT_EQUAL_PTR(&s_event.wait_map, s_task_a.wait_map_ptr);
}

void test_event_wait_blocking_wait_mask_stored(void)
{
    kernel_start_with_task_a();
    block_task_a(0x05U, xRTOS_EVENT_WAIT_ANY);
    TEST_ASSERT_EQUAL_UINT32(0x05U, s_task_a.block_payload.event.wait_mask);
}

void test_event_wait_blocking_wait_options_stored(void)
{
    kernel_start_with_task_a();
    uint32_t opts = xRTOS_EVENT_WAIT_ALL | xRTOS_EVENT_CLEAR_ON_EXIT;
    block_task_a(0x03U, opts);
    TEST_ASSERT_EQUAL_UINT32(opts, s_task_a.block_payload.event.wait_options);
}

void test_event_wait_blocking_timeout_map_armed(void)
{
    kernel_start_with_task_a();
    // Use finite timeout directly rather than the block_task_a helper.
    simulate_current_task(&s_task_a);
    (void)xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY, 10U, NULL);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

void test_event_wait_timeout_clears_task_payload(void)
{
    kernel_start_with_task_a();
    simulate_current_task(&s_task_a);
    (void)xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY, 5U, NULL);

    TEST_ASSERT_EQUAL_UINT32(0x01U, s_task_a.block_payload.event.wait_mask);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_EVENT_WAIT_ANY, s_task_a.block_payload.event.wait_options);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_EQUAL_UINT32(0U, s_task_a.block_payload.event.wait_mask);
    TEST_ASSERT_EQUAL_UINT32(0U, s_task_a.block_payload.event.wait_options);
    TEST_ASSERT_NULL(s_task_a.block_cleanup);
    TEST_ASSERT_NULL(s_task_a.block_cleanup_arg);
}

void test_event_wait_blocking_timeout_map_not_armed_for_wait_forever(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY); // WAIT_FOREVER
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

// SET - FLAG ACCUMULATION /////////////////////////////////////////////////////

void test_event_wait_blocking_uses_task_id_for_wait_state(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Event_Init(&s_event, NULL);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();

    (void)xRTOS_Event_Wait(&s_event, 0x05U, xRTOS_EVENT_WAIT_ANY, 10U, NULL);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_event.wait_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_event.wait_map, 3U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 3U));
    TEST_ASSERT_EQUAL_UINT32(0x05U, s_task_a.block_payload.event.wait_mask);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_EVENT_WAIT_ANY, s_task_a.block_payload.event.wait_options);
}

void test_event_set_null_ctx(void)
{
    xRTOS_Kernel_Context_t *k = &s_kernel;
    (void)k;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Event_Set(NULL, 0x01U));
}

void test_event_set_accumulates_flags(void)
{
    kernel_start_with_task_a();
    s_event.flags = 0x01U;
    (void)xRTOS_Event_Set(&s_event, 0x02U);
    TEST_ASSERT_EQUAL_UINT32(0x03U, s_event.flags);
}

void test_event_set_no_waiters_returns_ok(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Event_Set(&s_event, 0x01U));
}

// SET - UNICAST WAKE (WAIT_ANY) ///////////////////////////////////////////////

void test_event_set_wakes_wait_any_task(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    (void)xRTOS_Event_Set(&s_event, 0x01U);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_event_set_clears_wait_map_on_wake(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    (void)xRTOS_Event_Set(&s_event, 0x01U);
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_event.wait_map, TASK_A_ID));
}

void test_event_set_task_in_ready_map_after_wake(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    (void)xRTOS_Event_Set(&s_event, 0x01U);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.scheduler.ready_map, TASK_A_ID));
}

// SET - UNICAST WAKE (WAIT_ALL) ///////////////////////////////////////////////

void test_event_set_stores_matched_flags_in_waiter_payload(void)
{
    kernel_start_with_task_a();
    block_task_a(0x03U, xRTOS_EVENT_WAIT_ANY);
    (void)xRTOS_Event_Set(&s_event, 0x02U);
    TEST_ASSERT_EQUAL_UINT32(0x02U, s_task_a.block_payload.event.wait_mask);
}

void test_event_set_does_not_wake_when_all_bits_not_yet_set(void)
{
    kernel_start_with_task_a();
    block_task_a(0x03U, xRTOS_EVENT_WAIT_ALL);
    (void)xRTOS_Event_Set(&s_event, 0x01U); // only bit 0 - not all bits
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_event_set_wakes_wait_all_when_all_bits_set(void)
{
    kernel_start_with_task_a();
    block_task_a(0x03U, xRTOS_EVENT_WAIT_ALL);
    (void)xRTOS_Event_Set(&s_event, 0x01U);
    (void)xRTOS_Event_Set(&s_event, 0x02U); // now both bits set
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

// SET - CLEAR_ON_EXIT /////////////////////////////////////////////////////////

void test_event_set_clear_on_exit_removes_matched_bits(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT);
    s_event.flags = 0x03U;                  // pre-set bits 0 and 1
    (void)xRTOS_Event_Set(&s_event, 0x00U); // trigger scan without adding flags
    // Manually trigger re-scan by calling Set with 0: no new flags added but
    // existing flags satisfy the waiter. Actually Set ORs 0 -> flags unchanged,
    // then scans waiters. Bit 0 matches -> clear bit 0.
    TEST_ASSERT_EQUAL_UINT32(0x02U, s_event.flags); // bit 0 cleared, bit 1 stays
}

void test_event_set_no_clear_on_exit_preserves_flags(void)
{
    kernel_start_with_task_a();
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY); // no CLEAR_ON_EXIT
    (void)xRTOS_Event_Set(&s_event, 0x01U);
    TEST_ASSERT_EQUAL_UINT32(0x01U, s_event.flags); // bit 0 still set
}

// SET - MULTICAST /////////////////////////////////////////////////////////////

void test_event_set_multicast_wakes_both_tasks(void)
{
    kernel_start_with_task_b();

    // Block task_a waiting for bit 0.
    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY);
    // Block task_b waiting for bit 1.
    block_task_b(0x02U, xRTOS_EVENT_WAIT_ANY);

    // Set both bits from idle (patch current to idle).
    simulate_current_task(&s_idle_ctx);
    (void)xRTOS_Event_Set(&s_event, 0x03U);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
}

void test_event_set_clear_on_exit_multicast_uses_original_flags(void)
{
    kernel_start_with_task_b();

    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT);
    block_task_b(0x01U, xRTOS_EVENT_WAIT_ANY | xRTOS_EVENT_CLEAR_ON_EXIT);

    simulate_current_task(&s_idle_ctx);
    (void)xRTOS_Event_Set(&s_event, 0x01U);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_b.state);
    TEST_ASSERT_EQUAL_UINT32(0U, s_event.flags);
}

void test_event_set_partial_multicast_wakes_only_satisfied_task(void)
{
    kernel_start_with_task_b();

    block_task_a(0x01U, xRTOS_EVENT_WAIT_ANY); // waits for bit 0
    block_task_b(0x02U, xRTOS_EVENT_WAIT_ANY); // waits for bit 1

    simulate_current_task(&s_idle_ctx);
    (void)xRTOS_Event_Set(&s_event, 0x01U); // only bit 0

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_b.state);
}

// SET_FROM_ISR ///////////////////////////////////////////////////////////////

void test_event_set_from_isr_should_yield_true_when_waiter_outranks(void)
{
    kernel_start_with_task_b();

    // Block task_b (prio 5) on event.
    block_task_b(0x01U, xRTOS_EVENT_WAIT_ANY);

    // Simulate idle running; task_b woken by Set_From_ISR -> 5 < 31 -> yield.
    simulate_current_task(&s_idle_ctx);
    s_task_a.state = xRTOS_TASK_STATE_READY;

    bool yield = false;
    (void)xRTOS_Event_Set_From_ISR(&s_event, 0x01U, &yield);
    TEST_ASSERT_TRUE(yield);
}

void test_event_set_from_isr_should_yield_false_when_no_waiter(void)
{
    kernel_start_with_task_a();
    bool yield = false;
    (void)xRTOS_Event_Set_From_ISR(&s_event, 0x01U, &yield);
    TEST_ASSERT_FALSE(yield);
}

void test_event_set_from_isr_returns_ok(void)
{
    kernel_start_with_task_a();
    bool yield = false;
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Event_Set_From_ISR(&s_event, 0x01U, &yield));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Init
    RUN_TEST(test_event_init_null_ctx);
    RUN_TEST(test_event_init_flags_zero);
    RUN_TEST(test_event_init_wait_map_empty);

    // Clear
    RUN_TEST(test_event_clear_null_ctx);
    RUN_TEST(test_event_clear_clears_specified_bits);
    RUN_TEST(test_event_clear_does_not_affect_other_bits);
    RUN_TEST(test_event_clear_returns_ok);

    // Wait - fast path
    RUN_TEST(test_event_wait_fast_path_any_satisfied);
    RUN_TEST(test_event_wait_fast_path_all_satisfied);
    RUN_TEST(test_event_wait_fast_path_all_not_satisfied);
    RUN_TEST(test_event_wait_fast_path_clear_on_exit);
    RUN_TEST(test_event_wait_fast_path_no_wait_would_block);
    RUN_TEST(test_event_wait_rejects_zero_wait_mask);
    RUN_TEST(test_event_wait_rejects_missing_wait_mode);
    RUN_TEST(test_event_wait_rejects_both_wait_modes);

    // Wait - blocking setup
    RUN_TEST(test_event_wait_blocking_state_blocked);
    RUN_TEST(test_event_wait_blocking_in_wait_map);
    RUN_TEST(test_event_wait_blocking_wait_map_ptr_set);
    RUN_TEST(test_event_wait_blocking_wait_mask_stored);
    RUN_TEST(test_event_wait_blocking_wait_options_stored);
    RUN_TEST(test_event_wait_blocking_timeout_map_armed);
    RUN_TEST(test_event_wait_timeout_clears_task_payload);
    RUN_TEST(test_event_wait_blocking_timeout_map_not_armed_for_wait_forever);
    RUN_TEST(test_event_wait_blocking_uses_task_id_for_wait_state);

    // Set - flag accumulation
    RUN_TEST(test_event_set_null_ctx);
    RUN_TEST(test_event_set_accumulates_flags);
    RUN_TEST(test_event_set_no_waiters_returns_ok);

    // Set - unicast wake
    RUN_TEST(test_event_set_wakes_wait_any_task);
    RUN_TEST(test_event_set_clears_wait_map_on_wake);
    RUN_TEST(test_event_set_task_in_ready_map_after_wake);
    RUN_TEST(test_event_set_stores_matched_flags_in_waiter_payload);

    // Set - WAIT_ALL
    RUN_TEST(test_event_set_does_not_wake_when_all_bits_not_yet_set);
    RUN_TEST(test_event_set_wakes_wait_all_when_all_bits_set);

    // Set - CLEAR_ON_EXIT
    RUN_TEST(test_event_set_clear_on_exit_removes_matched_bits);
    RUN_TEST(test_event_set_no_clear_on_exit_preserves_flags);

    // Set - multicast
    RUN_TEST(test_event_set_multicast_wakes_both_tasks);
    RUN_TEST(test_event_set_clear_on_exit_multicast_uses_original_flags);
    RUN_TEST(test_event_set_partial_multicast_wakes_only_satisfied_task);

    // Set_From_ISR
    RUN_TEST(test_event_set_from_isr_should_yield_true_when_waiter_outranks);
    RUN_TEST(test_event_set_from_isr_should_yield_false_when_no_waiter);
    RUN_TEST(test_event_set_from_isr_returns_ok);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
