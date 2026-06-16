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

// @file test_xrtos_isr_guard.c
// @brief Host tests for release-safe ISR misuse guards on task-context APIs.

#include <stdbool.h>
#include <stdint.h>

static void test_assert_hook(void);
#define xASSERT_HOOK() test_assert_hook()

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_event.h"
#include "xrtos_mutex.h"
#include "xrtos_notify.h"
#include "xrtos_queue.h"
#include "xrtos_return.h"
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;

static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];

static xRTOS_Task_Context_t s_task_ctx;
static uint32_t s_task_stack[64U];

static xRTOS_Sem_Context_t s_sem;
static xRTOS_Mutex_Context_t s_mutex;
static xRTOS_Event_Context_t s_event;
static xRTOS_Queue_Context_t s_queue;
static uint32_t s_queue_storage[2U];

static bool s_is_in_isr;
static uint32_t s_yield_count;
static uint32_t s_assert_count;

// Override the weak assert hook from xassert.h so this test can verify debug
// assertions without halting the process.
void xassert_system_halt(void)
{
    s_assert_count++;
}

static void test_assert_hook(void)
{
    s_assert_count++;
}

static void dummy_entry(void *arg)
{
    (void)arg;
}

static void test_init_task_stack(xRTOS_Task_Context_t *task_ctx, xRTOS_Task_Entry_t entry, void *arg)
{
    (void)entry;
    (void)arg;
    task_ctx->stack_top = task_ctx->stack_mem + task_ctx->stack_words;
}

static void test_start_first_task(xRTOS_Task_Context_t *task_ctx)
{
    (void)task_ctx;
}

static void test_yield(void)
{
    s_yield_count++;
}

static uint32_t test_disable_interrupts(void)
{
    return 0U;
}

static void test_enable_interrupts(uint32_t saved_state)
{
    (void)saved_state;
}

static bool test_is_in_isr(void)
{
    return s_is_in_isr;
}

static const xRTOS_Port_Ops_t s_port_ops = {
    .init_task_stack = test_init_task_stack,
    .start_first_task = test_start_first_task,
    .yield = test_yield,
    .disable_interrupts = test_disable_interrupts,
    .enable_interrupts = test_enable_interrupts,
    .is_in_isr = test_is_in_isr,
};

static void register_task(uint32_t task_id, uint32_t priority, xRTOS_Task_Context_t *ctx, uint32_t *stack)
{
    xRTOS_Task_Config_t cfg = {
        .task_id = task_id,
        .priority = priority,
        .entry = dummy_entry,
        .entry_arg = NULL,
        .stack_mem = stack,
        .stack_words = 64U,
    };
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Task_Create(ctx, &cfg));
}

void setUp(void)
{
    s_is_in_isr = false;
    s_yield_count = 0U;
    s_assert_count = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Kernel_Init(&s_kernel, &s_port_ops));
    register_task(xRTOS_IDLE_TASK_ID, xRTOS_IDLE_PRIORITY, &s_idle_ctx, s_idle_stack);
    register_task(1U, 1U, &s_task_ctx, s_task_stack);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Kernel_Start());

    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Sem_Init(&s_sem, 0U, 1U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Mutex_Init(&s_mutex, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Event_Init(&s_event, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Queue_Init(&s_queue, s_queue_storage, sizeof(uint32_t), 2U, NULL));

    s_is_in_isr = true;
}

void tearDown(void)
{
}

void test_task_delay_from_isr_returns_invalid_state(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Task_Delay(1U));
}

void test_sem_take_from_isr_returns_invalid_state(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Sem_Take(&s_sem, xRTOS_WAIT_FOREVER));
}

void test_mutex_lock_from_isr_returns_invalid_state(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER));
}

void test_event_wait_from_isr_returns_invalid_state(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Event_Wait(&s_event, 0x01U, xRTOS_EVENT_WAIT_ANY, xRTOS_WAIT_FOREVER, NULL));
}

void test_queue_send_from_isr_context_returns_invalid_state(void)
{
    uint32_t item = 42U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Queue_Send(&s_queue, &item, xRTOS_NO_WAIT));
}

void test_queue_receive_from_isr_context_returns_invalid_state(void)
{
    uint32_t item = 0U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Queue_Receive(&s_queue, &item, xRTOS_WAIT_FOREVER));
}

void test_notify_wait_from_isr_returns_invalid_state(void)
{
    uint32_t value = 0U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_INVALID_STATE, xRTOS_Task_Notify_Wait(0U, 0U, &value, xRTOS_WAIT_FOREVER));
}

void test_task_yield_from_isr_does_not_call_port_yield(void)
{
    xRTOS_Task_Yield();

    TEST_ASSERT_EQUAL_UINT32(0U, s_yield_count);
}

void test_blocking_guard_triggers_debug_assertion(void)
{
    (void)xRTOS_Task_Delay(1U);

    TEST_ASSERT_TRUE(s_assert_count >= 1U);
}

void test_from_isr_null_should_yield_returns_null_pointer(void)
{
    uint32_t item = 1U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Sem_Give_From_ISR(&s_sem, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Event_Set_From_ISR(&s_event, 0x01U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Task_Notify_From_ISR(1U, 0x01U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xRTOS_NULL_POINTER, xRTOS_Queue_Send_From_ISR(&s_queue, &item, NULL));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_task_delay_from_isr_returns_invalid_state);
    RUN_TEST(test_sem_take_from_isr_returns_invalid_state);
    RUN_TEST(test_mutex_lock_from_isr_returns_invalid_state);
    RUN_TEST(test_event_wait_from_isr_returns_invalid_state);
    RUN_TEST(test_queue_send_from_isr_context_returns_invalid_state);
    RUN_TEST(test_queue_receive_from_isr_context_returns_invalid_state);
    RUN_TEST(test_notify_wait_from_isr_returns_invalid_state);
    RUN_TEST(test_task_yield_from_isr_does_not_call_port_yield);
    RUN_TEST(test_blocking_guard_triggers_debug_assertion);
    RUN_TEST(test_from_isr_null_should_yield_returns_null_pointer);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
