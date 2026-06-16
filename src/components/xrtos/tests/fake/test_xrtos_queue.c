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

// @file test_xrtos_queue.c
// @brief Host tests for xRTOS fixed-size queue (Phase 15).
//
// Tests cover:
//   1. xRTOS_Queue_Init    - field initialization and argument validation.
//   2. xRTOS_Queue_Send    - fast path (space available), WOULD_BLOCK on NO_WAIT,
//      ring-buffer mechanics (tail advance, wrap), blocking setup (send_wait_map,
//      state, timeout_map arming).
//   3. xRTOS_Queue_Receive - fast path (item available), WOULD_BLOCK on NO_WAIT,
//      ring-buffer mechanics (head advance), blocking setup (recv_wait_map, state).
//   4. Cross-primitive wake - Send wakes a blocked receiver; Receive wakes a
//      blocked sender.
//   5. xRTOS_Queue_Send_From_ISR - ISR enqueue with should_yield output.
//
// Host simulation note:
//   xRTOS_Scheduler_Block_Current does not perform a real context switch on the
//   host. Queue_Send on a full queue and Queue_Receive on an empty queue transition
//   the current task to BLOCKED and return immediately. Tests that verify blocking
//   SETUP check intermediate bitmap and state fields after the call returns.
//   Wake-path tests call the complementary primitive immediately after and verify
//   the resulting READY state and schedule-pending flag.
//

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_queue.h"
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

static xRTOS_Queue_Context_t s_queue;
static uint32_t s_storage[4U]; // 4 x uint32_t items
static uint32_t s_blocked_send_value;
static uint32_t s_blocked_recv_value;

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
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Register idle + task_a + task_b, start kernel (task_b RUNNING at prio 5).
static void kernel_start_with_task_b(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task(&s_task_a, s_stack_a, TASK_A_PRIORITY);
    (void)register_task(&s_task_b, s_stack_b, TASK_B_PRIORITY);
    (void)xRTOS_Kernel_Start();
}

// Make task_a the current running task and block it on recv_wait_map (queue empty).
static void block_task_a_on_receive(void)
{
    simulate_current_task(&s_task_a);
    s_blocked_recv_value = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &s_blocked_recv_value, xRTOS_WAIT_FOREVER);
}

// Fill the queue to capacity then block task_a on send_wait_map.
static void fill_queue_and_block_task_a_on_send(void)
{
    uint32_t val = 0xAAU;
    s_blocked_send_value = 0x55AAU;

    // Fill to capacity using Send fast path.
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }
    // Now block task_a trying to send one more.
    simulate_current_task(&s_task_a);
    (void)xRTOS_Queue_Send(&s_queue, &s_blocked_send_value, xRTOS_WAIT_FOREVER);
}

// SETUP / TEARDOWN ////////////////////////////////////////////////////////////

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
}

void tearDown(void)
{
}

// INIT TESTS //////////////////////////////////////////////////////////////////

void test_queue_init_null_queue_ctx(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Init(NULL, s_storage, sizeof(uint32_t), 4U, NULL));
}

void test_queue_init_null_storage(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Init(&s_queue, NULL, sizeof(uint32_t), 4U, NULL));
}

void test_queue_init_zero_item_size(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Queue_Init(&s_queue, s_storage, 0U, 4U, NULL));
}

void test_queue_init_zero_item_count(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 0U, NULL));
}

void test_queue_init_rejects_index_overflow(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, xRTOS_Queue_Init(&s_queue, s_storage, 2U, UINT32_MAX, NULL));
}

void test_queue_init_sets_storage(void)
{
    TEST_ASSERT_EQUAL_PTR(s_storage, s_queue.storage);
}

void test_queue_init_sets_item_size(void)
{
    TEST_ASSERT_EQUAL_UINT32(sizeof(uint32_t), s_queue.item_size);
}

void test_queue_init_sets_item_count(void)
{
    TEST_ASSERT_EQUAL_UINT32(4U, s_queue.item_count);
}

void test_queue_init_zeros_head(void)
{
    s_queue.head = 0xDEADU;
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, s_queue.head);
}

void test_queue_init_zeros_tail(void)
{
    s_queue.tail = 0xDEADU;
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, s_queue.tail);
}

void test_queue_init_zeros_used(void)
{
    s_queue.used = 99U;
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    TEST_ASSERT_EQUAL_UINT32(0U, s_queue.used);
}

void test_queue_init_clears_send_wait_map(void)
{
    s_queue.send_wait_map.words[0] = 0xFFFFFFFFU;
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_queue.send_wait_map));
}

void test_queue_init_clears_recv_wait_map(void)
{
    s_queue.recv_wait_map.words[0] = 0xFFFFFFFFU;
    (void)xRTOS_Queue_Init(&s_queue, s_storage, sizeof(uint32_t), 4U, NULL);
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Empty(&s_queue.recv_wait_map));
}

// SEND - FAST PATH ////////////////////////////////////////////////////////////

void test_queue_send_returns_ok_when_space_available(void)
{
    kernel_start_with_task_a();
    uint32_t val = 42U;

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT));
}

void test_queue_send_increments_used(void)
{
    kernel_start_with_task_a();
    uint32_t val = 1U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(1U, s_queue.used);
}

void test_queue_send_advances_tail(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(1U, s_queue.tail);
}

void test_queue_send_stores_data_in_storage(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0xCAFEU;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(0xCAFEU, s_storage[0]);
}

void test_queue_send_tail_wraps_at_item_count(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    // Fill all 4 slots.
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }
    // tail should have wrapped back to 0.
    TEST_ASSERT_EQUAL_UINT32(0U, s_queue.tail);
}

void test_queue_send_null_queue_ctx(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Send(NULL, &val, xRTOS_NO_WAIT));
}

void test_queue_send_null_item(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Send(&s_queue, NULL, xRTOS_NO_WAIT));
}

// SEND - WOULD_BLOCK / BLOCKING ///////////////////////////////////////////////

void test_queue_send_returns_would_block_when_full_no_wait(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT));
}

void test_queue_send_blocking_sets_send_wait_map_bit(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send();

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_queue.send_wait_map, TASK_A_ID));
}

void test_queue_send_blocking_transitions_task_to_blocked(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send();

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_queue_send_blocking_arms_timeout_map(void)
{
    kernel_start_with_task_b();
    uint32_t val = 0xBBU;
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }
    simulate_current_task(&s_task_a);
    (void)xRTOS_Queue_Send(&s_queue, &val, 50U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

void test_queue_send_timeout_clears_saved_item_pointer(void)
{
    kernel_start_with_task_b();
    uint32_t val = 0xBBU;
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }

    s_blocked_send_value = 0x1234U;
    simulate_current_task(&s_task_a);
    (void)xRTOS_Queue_Send(&s_queue, &s_blocked_send_value, 5U);

    TEST_ASSERT_EQUAL_PTR(&s_blocked_send_value, s_task_a.block_payload.const_ptr);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_NULL(s_task_a.block_payload.ptr);
    TEST_ASSERT_NULL(s_task_a.block_cleanup);
    TEST_ASSERT_NULL(s_task_a.block_cleanup_arg);
}

void test_queue_send_blocking_does_not_arm_timeout_map_forever(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send(); // uses WAIT_FOREVER

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

// RECEIVE - FAST PATH /////////////////////////////////////////////////////////

void test_queue_send_blocking_uses_task_id_for_wait_state(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();

    uint32_t val = 0xBBU;
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    }

    s_blocked_send_value = 0x1234U;
    (void)xRTOS_Queue_Send(&s_queue, &s_blocked_send_value, 10U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_queue.send_wait_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_queue.send_wait_map, 3U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 3U));
    TEST_ASSERT_EQUAL_PTR(&s_blocked_send_value, s_task_a.block_payload.const_ptr);
}

void test_queue_receive_returns_ok_when_item_available(void)
{
    kernel_start_with_task_a();
    uint32_t val = 7U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    uint32_t out = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT));
}

void test_queue_receive_decrements_used(void)
{
    kernel_start_with_task_a();
    uint32_t val = 1U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(1U, s_queue.used);
}

void test_queue_receive_advances_head(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(1U, s_queue.head);
}

void test_queue_receive_copies_correct_data(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0xBEEFU;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(0xBEEFU, out);
}

void test_queue_receive_fifo_order(void)
{
    kernel_start_with_task_a();
    uint32_t a = 10U, b = 20U, c = 30U;
    (void)xRTOS_Queue_Send(&s_queue, &a, xRTOS_NO_WAIT);
    (void)xRTOS_Queue_Send(&s_queue, &b, xRTOS_NO_WAIT);
    (void)xRTOS_Queue_Send(&s_queue, &c, xRTOS_NO_WAIT);
    uint32_t out1 = 0U, out2 = 0U, out3 = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out1, xRTOS_NO_WAIT);
    (void)xRTOS_Queue_Receive(&s_queue, &out2, xRTOS_NO_WAIT);
    (void)xRTOS_Queue_Receive(&s_queue, &out3, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(10U, out1);
    TEST_ASSERT_EQUAL_UINT32(20U, out2);
    TEST_ASSERT_EQUAL_UINT32(30U, out3);
}

void test_queue_receive_null_queue_ctx(void)
{
    kernel_start_with_task_a();
    uint32_t out = 0U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Receive(NULL, &out, xRTOS_NO_WAIT));
}

void test_queue_receive_null_item(void)
{
    kernel_start_with_task_a();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Receive(&s_queue, NULL, xRTOS_NO_WAIT));
}

// RECEIVE - WOULD_BLOCK / BLOCKING ////////////////////////////////////////////

void test_queue_receive_returns_would_block_when_empty_no_wait(void)
{
    kernel_start_with_task_a();
    uint32_t out = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_WOULD_BLOCK, xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT));
}

void test_queue_receive_blocking_sets_recv_wait_map_bit(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive();

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_queue.recv_wait_map, TASK_A_ID));
}

void test_queue_receive_blocking_transitions_task_to_blocked(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive();

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_BLOCKED, s_task_a.state);
}

void test_queue_receive_blocking_arms_timeout_map(void)
{
    kernel_start_with_task_b();
    simulate_current_task(&s_task_a);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, 100U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

void test_queue_receive_timeout_clears_saved_buffer_pointer(void)
{
    kernel_start_with_task_b();
    simulate_current_task(&s_task_a);
    s_blocked_recv_value = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &s_blocked_recv_value, 5U);

    TEST_ASSERT_EQUAL_PTR(&s_blocked_recv_value, s_task_a.block_payload.ptr);

    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    bool yield = false;
    for (uint32_t i = 0U; i < 5U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    TEST_ASSERT_NULL(s_task_a.block_payload.ptr);
    TEST_ASSERT_NULL(s_task_a.block_cleanup);
    TEST_ASSERT_NULL(s_task_a.block_cleanup_arg);
}

void test_queue_receive_blocking_does_not_arm_timeout_map_forever(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive(); // uses WAIT_FOREVER

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, TASK_A_ID));
}

// CROSS-PRIMITIVE WAKE ////////////////////////////////////////////////////////

void test_queue_receive_blocking_uses_task_id_for_wait_state(void)
{
    (void)register_task(&s_idle_ctx, s_idle_stack, IDLE_PRIORITY);
    (void)register_task_with_id(&s_task_a, s_stack_a, 9U, 3U);
    (void)xRTOS_Kernel_Start();

    s_blocked_recv_value = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &s_blocked_recv_value, 10U);

    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_queue.recv_wait_map, 9U));
    TEST_ASSERT_TRUE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 9U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_queue.recv_wait_map, 3U));
    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_kernel.timeout_map, 3U));
    TEST_ASSERT_EQUAL_PTR(&s_blocked_recv_value, s_task_a.block_payload.ptr);
}

void test_queue_send_wakes_blocked_receiver(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive(); // task_a BLOCKED on recv_wait_map

    // task_b sends an item - should wake task_a.
    simulate_current_task(&s_task_b);
    uint32_t val = 99U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_queue_send_copies_item_to_blocked_receiver_buffer(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive();

    simulate_current_task(&s_task_b);
    uint32_t val = 99U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(99U, s_blocked_recv_value);
    TEST_ASSERT_EQUAL_UINT32(0U, s_queue.used);
}

void test_queue_send_wakes_receiver_clears_recv_wait_map_bit(void)
{
    kernel_start_with_task_b();
    block_task_a_on_receive();

    simulate_current_task(&s_task_b);
    uint32_t val = 0U;
    (void)xRTOS_Queue_Send(&s_queue, &val, xRTOS_NO_WAIT);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_queue.recv_wait_map, TASK_A_ID));
}

void test_queue_receive_wakes_blocked_sender(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send(); // task_a BLOCKED on send_wait_map

    // task_b receives an item - should wake task_a.
    simulate_current_task(&s_task_b);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL(xRTOS_TASK_STATE_READY, s_task_a.state);
}

void test_queue_receive_enqueues_blocked_sender_item(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send();

    simulate_current_task(&s_task_b);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_EQUAL_UINT32(0xAAU, out);
    TEST_ASSERT_EQUAL_UINT32(4U, s_queue.used);
    TEST_ASSERT_EQUAL_UINT32(s_blocked_send_value, s_storage[0]);
}

void test_queue_receive_wakes_sender_clears_send_wait_map_bit(void)
{
    kernel_start_with_task_b();
    fill_queue_and_block_task_a_on_send();

    simulate_current_task(&s_task_b);
    uint32_t out = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &out, xRTOS_NO_WAIT);

    TEST_ASSERT_FALSE(xRTOS_Bitmap_Is_Set(&s_queue.send_wait_map, TASK_A_ID));
}

// SEND_FROM_ISR ///////////////////////////////////////////////////////////////

void test_queue_send_from_isr_null_queue_ctx(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
    uint32_t val = 0U;
    bool yld = false;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Send_From_ISR(NULL, &val, &yld));
}

void test_queue_send_from_isr_enqueues_item(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0xF0F0U;
    bool yld = false;
    (void)xRTOS_Queue_Send_From_ISR(&s_queue, &val, &yld);

    TEST_ASSERT_EQUAL_UINT32(1U, s_queue.used);
    TEST_ASSERT_EQUAL_UINT32(0xF0F0U, s_storage[0]);
}

void test_queue_send_from_isr_returns_resource_full_when_queue_full(void)
{
    kernel_start_with_task_a();
    uint32_t val = 0U;
    bool yld = false;
    for (uint32_t i = 0U; i < 4U; i++)
    {
        (void)xRTOS_Queue_Send_From_ISR(&s_queue, &val, &yld);
    }

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_RESOURCE_FULL, xRTOS_Queue_Send_From_ISR(&s_queue, &val, &yld));
}

void test_queue_send_from_isr_should_yield_true_when_higher_prio_receiver_woken(void)
{
    kernel_start_with_task_b();

    // Block task_b (prio=5, higher priority) on an empty queue receive.
    simulate_current_task(&s_task_b);
    uint32_t dummy = 0U;
    (void)xRTOS_Queue_Receive(&s_queue, &dummy, xRTOS_WAIT_FOREVER);

    // Simulate ISR firing while task_a (prio=10, lower priority) is current.
    simulate_current_task(&s_task_a);

    uint32_t val = 1U;
    bool yld = false;
    (void)xRTOS_Queue_Send_From_ISR(&s_queue, &val, &yld);

    TEST_ASSERT_TRUE(yld);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    // Init
    RUN_TEST(test_queue_init_null_queue_ctx);
    RUN_TEST(test_queue_init_null_storage);
    RUN_TEST(test_queue_init_zero_item_size);
    RUN_TEST(test_queue_init_zero_item_count);
    RUN_TEST(test_queue_init_rejects_index_overflow);
    RUN_TEST(test_queue_init_sets_storage);
    RUN_TEST(test_queue_init_sets_item_size);
    RUN_TEST(test_queue_init_sets_item_count);
    RUN_TEST(test_queue_init_zeros_head);
    RUN_TEST(test_queue_init_zeros_tail);
    RUN_TEST(test_queue_init_zeros_used);
    RUN_TEST(test_queue_init_clears_send_wait_map);
    RUN_TEST(test_queue_init_clears_recv_wait_map);

    // Send - fast path
    RUN_TEST(test_queue_send_returns_ok_when_space_available);
    RUN_TEST(test_queue_send_increments_used);
    RUN_TEST(test_queue_send_advances_tail);
    RUN_TEST(test_queue_send_stores_data_in_storage);
    RUN_TEST(test_queue_send_tail_wraps_at_item_count);
    RUN_TEST(test_queue_send_null_queue_ctx);
    RUN_TEST(test_queue_send_null_item);

    // Send - blocking
    RUN_TEST(test_queue_send_returns_would_block_when_full_no_wait);
    RUN_TEST(test_queue_send_blocking_sets_send_wait_map_bit);
    RUN_TEST(test_queue_send_blocking_transitions_task_to_blocked);
    RUN_TEST(test_queue_send_blocking_arms_timeout_map);
    RUN_TEST(test_queue_send_timeout_clears_saved_item_pointer);
    RUN_TEST(test_queue_send_blocking_does_not_arm_timeout_map_forever);
    RUN_TEST(test_queue_send_blocking_uses_task_id_for_wait_state);

    // Receive - fast path
    RUN_TEST(test_queue_receive_returns_ok_when_item_available);
    RUN_TEST(test_queue_receive_decrements_used);
    RUN_TEST(test_queue_receive_advances_head);
    RUN_TEST(test_queue_receive_copies_correct_data);
    RUN_TEST(test_queue_receive_fifo_order);
    RUN_TEST(test_queue_receive_null_queue_ctx);
    RUN_TEST(test_queue_receive_null_item);

    // Receive - blocking
    RUN_TEST(test_queue_receive_returns_would_block_when_empty_no_wait);
    RUN_TEST(test_queue_receive_blocking_sets_recv_wait_map_bit);
    RUN_TEST(test_queue_receive_blocking_transitions_task_to_blocked);
    RUN_TEST(test_queue_receive_blocking_arms_timeout_map);
    RUN_TEST(test_queue_receive_timeout_clears_saved_buffer_pointer);
    RUN_TEST(test_queue_receive_blocking_does_not_arm_timeout_map_forever);
    RUN_TEST(test_queue_receive_blocking_uses_task_id_for_wait_state);

    // Cross-primitive wake
    RUN_TEST(test_queue_send_wakes_blocked_receiver);
    RUN_TEST(test_queue_send_copies_item_to_blocked_receiver_buffer);
    RUN_TEST(test_queue_send_wakes_receiver_clears_recv_wait_map_bit);
    RUN_TEST(test_queue_receive_wakes_blocked_sender);
    RUN_TEST(test_queue_receive_enqueues_blocked_sender_item);
    RUN_TEST(test_queue_receive_wakes_sender_clears_send_wait_map_bit);

    // Send_From_ISR
    RUN_TEST(test_queue_send_from_isr_null_queue_ctx);
    RUN_TEST(test_queue_send_from_isr_enqueues_item);
    RUN_TEST(test_queue_send_from_isr_returns_resource_full_when_queue_full);
    RUN_TEST(test_queue_send_from_isr_should_yield_true_when_higher_prio_receiver_woken);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
