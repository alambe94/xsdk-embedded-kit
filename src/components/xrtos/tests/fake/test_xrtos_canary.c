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

// @file test_xrtos_canary.c
// @brief Host tests for xRTOS stack canary placement and overflow detection.
//
// Tests verify the predicates used by the scheduler's context-switch canary
// check without relying on xASSERT firing (which would halt the process).
// The scheduler integration itself is covered in Phase 4 (test_xrtos_scheduler.c).
//

#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_task.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel_ctx;
static xRTOS_Task_Context_t s_task_ctx;
static uint32_t s_stack[64U];

static void dummy_task_entry(void *arg)
{
    (void)arg;
}

static xRTOS_Task_Config_t make_task_config(uint32_t task_id, uint32_t priority)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = task_id;
    cfg.priority = priority;
    cfg.entry = dummy_task_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = s_stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return cfg;
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel_ctx, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: canary placement /////////////////////////////////////////////////////

void test_canary_is_written_at_stack_base_on_task_creation(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_HEX32(xRTOS_STACK_CANARY, s_stack[0U]);
}

void test_canary_value_is_dead_beef(void)
{
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFU, xRTOS_STACK_CANARY);
}

void test_canary_is_at_word_zero_not_shifted(void)
{
    // Verify the canary occupies exactly stack_mem[0] and not any other word.
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    s_stack[1U] = 0xFFFFFFFFU;
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_EQUAL_HEX32(xRTOS_STACK_CANARY, s_stack[0U]);
    // The word above the canary must not have been touched by canary placement.
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFFU, s_stack[1U]);
}

// TESTS: xRTOS_Task_Stack_Is_Valid ////////////////////////////////////////////

void test_stack_is_valid_returns_true_after_task_creation(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_TRUE(xRTOS_Task_Stack_Is_Valid(&s_task_ctx));
}

void test_stack_is_valid_returns_false_when_canary_corrupted(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);

    // Simulate a deep stack overflow that overwrites the canary word.
    s_task_ctx.stack_mem[0U] = 0U;

    TEST_ASSERT_FALSE(xRTOS_Task_Stack_Is_Valid(&s_task_ctx));
}

void test_stack_is_valid_returns_false_when_stack_top_below_base(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);

    // Simulate a stack pointer that has been pushed below the allocation base,
    // as would happen after a severe overflow on a downward-growing stack.
    s_task_ctx.stack_top = s_task_ctx.stack_mem - 1U;

    TEST_ASSERT_FALSE(xRTOS_Task_Stack_Is_Valid(&s_task_ctx));
}

void test_stack_top_is_within_bounds_after_fake_port_init(void)
{
    // The fake port sets stack_top = stack_mem + stack_words (past the top).
    // Verify stack_top is at or above stack_mem (i.e. no wrap-around).
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    TEST_ASSERT_GREATER_OR_EQUAL(s_task_ctx.stack_mem, s_task_ctx.stack_top);
}

void test_stack_top_does_not_exceed_stack_allocation(void)
{
    xRTOS_Task_Config_t cfg = make_task_config(0U, 0U);
    (void)xRTOS_Task_Create(&s_task_ctx, &cfg);
    // stack_top must not point past the allocated buffer end.
    const uint32_t *stack_end = s_task_ctx.stack_mem + s_task_ctx.stack_words;
    TEST_ASSERT_LESS_OR_EQUAL(stack_end, s_task_ctx.stack_top);
}

// TESTS: canary survives normal use ///////////////////////////////////////////

void test_canary_survives_multiple_task_registrations(void)
{
    static xRTOS_Task_Context_t t1, t2;
    static uint32_t s1[64U], s2[64U];

    xRTOS_Task_Config_t c1 = {0U, 0U, dummy_task_entry, NULL, s1, 64U, NULL};
    xRTOS_Task_Config_t c2 = {1U, 1U, dummy_task_entry, NULL, s2, 64U, NULL};

    (void)xRTOS_Task_Create(&t1, &c1);
    (void)xRTOS_Task_Create(&t2, &c2);

    TEST_ASSERT_EQUAL_HEX32(xRTOS_STACK_CANARY, s1[0U]);
    TEST_ASSERT_EQUAL_HEX32(xRTOS_STACK_CANARY, s2[0U]);
    TEST_ASSERT_TRUE(xRTOS_Task_Stack_Is_Valid(&t1));
    TEST_ASSERT_TRUE(xRTOS_Task_Stack_Is_Valid(&t2));
}

void test_canary_is_independent_per_task_stack(void)
{
    static xRTOS_Task_Context_t t1, t2;
    static uint32_t s1[64U], s2[64U];

    xRTOS_Task_Config_t c1 = {0U, 0U, dummy_task_entry, NULL, s1, 64U, NULL};
    xRTOS_Task_Config_t c2 = {1U, 1U, dummy_task_entry, NULL, s2, 64U, NULL};

    (void)xRTOS_Task_Create(&t1, &c1);
    (void)xRTOS_Task_Create(&t2, &c2);

    // Corrupt only task 1's stack.
    s1[0U] = 0xBADC0FFEU;

    TEST_ASSERT_FALSE(xRTOS_Task_Stack_Is_Valid(&t1));
    TEST_ASSERT_TRUE(xRTOS_Task_Stack_Is_Valid(&t2));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_canary_is_written_at_stack_base_on_task_creation);
    RUN_TEST(test_canary_value_is_dead_beef);
    RUN_TEST(test_canary_is_at_word_zero_not_shifted);

    RUN_TEST(test_stack_is_valid_returns_true_after_task_creation);
    RUN_TEST(test_stack_is_valid_returns_false_when_canary_corrupted);
    RUN_TEST(test_stack_is_valid_returns_false_when_stack_top_below_base);
    RUN_TEST(test_stack_top_is_within_bounds_after_fake_port_init);
    RUN_TEST(test_stack_top_does_not_exceed_stack_allocation);

    RUN_TEST(test_canary_survives_multiple_task_registrations);
    RUN_TEST(test_canary_is_independent_per_task_stack);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
