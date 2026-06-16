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

// @file test_xrtos_config_small.c
// @brief Header-only checks for small xRTOS task/priority/timer configuration.

#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_small_config_constants_are_decoupled(void)
{
    TEST_ASSERT_EQUAL_UINT32(8U, xRTOS_MAX_TASKS);
    TEST_ASSERT_EQUAL_UINT32(12U, xRTOS_MAX_PRIORITIES);
    TEST_ASSERT_EQUAL_UINT32(4U, xRTOS_MAX_TIMERS);
    TEST_ASSERT_EQUAL_UINT32(12U, xRTOS_BITMAP_WIDTH);
    TEST_ASSERT_EQUAL_UINT32(1U, xRTOS_BITMAP_WORD_COUNT);
}

void test_small_config_idle_task_id_and_priority_are_independent(void)
{
    TEST_ASSERT_EQUAL_UINT32(7U, xRTOS_IDLE_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(11U, xRTOS_IDLE_PRIORITY);
    TEST_ASSERT_EQUAL_UINT32(10U, xRTOS_LOWEST_USER_PRIORITY);
}

void test_small_config_kernel_tables_use_specific_limits(void)
{
    size_t task_table_bytes = sizeof(((xRTOS_Kernel_Context_t *)0)->task_table);
    size_t priority_table_bytes = sizeof(((xRTOS_Kernel_Context_t *)0)->task_by_priority);
    size_t timer_table_bytes = sizeof(((xRTOS_Kernel_Context_t *)0)->timer_table);

    TEST_ASSERT_EQUAL_UINT32(8U * (uint32_t)sizeof(xRTOS_Task_Context_t *), (uint32_t)task_table_bytes);
    TEST_ASSERT_EQUAL_UINT32(12U * (uint32_t)sizeof(xRTOS_Task_Context_t *), (uint32_t)priority_table_bytes);
    TEST_ASSERT_EQUAL_UINT32(4U * (uint32_t)sizeof(struct xRTOS_Timer_Context_t *), (uint32_t)timer_table_bytes);
}

void test_small_config_scheduler_ready_lists_use_priority_limit(void)
{
    size_t ready_head_bytes = sizeof(((xRTOS_Scheduler_Context_t *)0)->ready_head);
    size_t ready_tail_bytes = sizeof(((xRTOS_Scheduler_Context_t *)0)->ready_tail);

    TEST_ASSERT_EQUAL_UINT32(12U * (uint32_t)sizeof(xRTOS_Task_Context_t *), (uint32_t)ready_head_bytes);
    TEST_ASSERT_EQUAL_UINT32(12U * (uint32_t)sizeof(xRTOS_Task_Context_t *), (uint32_t)ready_tail_bytes);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_small_config_constants_are_decoupled);
    RUN_TEST(test_small_config_idle_task_id_and_priority_are_independent);
    RUN_TEST(test_small_config_kernel_tables_use_specific_limits);
    RUN_TEST(test_small_config_scheduler_ready_lists_use_priority_limit);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
