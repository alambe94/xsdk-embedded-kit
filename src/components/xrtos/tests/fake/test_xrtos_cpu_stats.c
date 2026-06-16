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

// @file test_xrtos_cpu_stats.c
// @brief Host tests for xRTOS per-task CPU usage statistics (Phase 24b).
//
// This file is compiled twice:
//   xrtos_cpu_stats_disabled_test - xRTOS_CONFIG_CPU_STATS_ENABLE=0 (default)
//   xrtos_cpu_stats_test          - xRTOS_CONFIG_CPU_STATS_ENABLE=1
// Tests that require the feature enabled are gated with #if.
//

#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xrtos_config.h"
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_fake.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_task0;
static uint32_t s_stack0[64U];
#if xRTOS_CONFIG_CPU_STATS_ENABLE
static xRTOS_Task_Context_t s_task1;
static uint32_t s_stack1[64U];
#endif

static void dummy_entry(void *arg)
{
    (void)arg;
}

static xRTOS_Task_Config_t make_cfg(uint32_t task_id, uint32_t priority, uint32_t *stack)
{
    xRTOS_Task_Config_t cfg;
    cfg.task_id = task_id;
    cfg.priority = priority;
    cfg.entry = dummy_entry;
    cfg.entry_arg = NULL;
    cfg.stack_mem = stack;
    cfg.stack_words = 64U;
    cfg.name = NULL;
    return cfg;
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel, &xrtos_fake_port_ops);
}

void tearDown(void)
{
}

// TESTS: disabled path ////////////////////////////////////////////////////////

void test_get_cpu_stats_returns_invalid_state_when_disabled(void)
{
#if xRTOS_CONFIG_CPU_STATS_ENABLE
    TEST_IGNORE_MESSAGE("skipped: CPU stats enabled in this build");
#else
    xRTOS_Task_Config_t cfg = make_cfg(0U, 0U, s_stack0);
    (void)xRTOS_Task_Create(&s_task0, &cfg);

    xRTOS_Task_CPU_Stats_t out;
    xRETURN_t ret = xRTOS_Task_Get_CPU_Stats(0U, &out);
    TEST_ASSERT_EQUAL_INT(xRETURN_xERR_xRTOS_INVALID_STATE, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out.total_ticks);
#endif
}

void test_reset_all_is_noop_when_disabled(void)
{
    // Must not crash; no assertions about state since the feature is off.
    xRTOS_CPU_Stats_Reset_All();
}

// TESTS: enabled path /////////////////////////////////////////////////////////

#if xRTOS_CONFIG_CPU_STATS_ENABLE

void test_ticks_running_starts_at_zero(void)
{
    xRTOS_Task_Config_t cfg = make_cfg(0U, 0U, s_stack0);
    (void)xRTOS_Task_Create(&s_task0, &cfg);

    xRTOS_Task_CPU_Stats_t out;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out);
    TEST_ASSERT_EQUAL_UINT32(0U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out.total_ticks);
}

void test_tick_increments_current_task_counter(void)
{
    xRTOS_Task_Config_t cfg = make_cfg(0U, 0U, s_stack0);
    (void)xRTOS_Task_Create(&s_task0, &cfg);

    // Drive ticks while task 0 is the current task.
    s_kernel.scheduler.current_task_id = 0U;
    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);

    xRTOS_Task_CPU_Stats_t out;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out);
    TEST_ASSERT_EQUAL_UINT32(3U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(3U, out.total_ticks);
}

void test_only_current_task_counter_increments(void)
{
    xRTOS_Task_Config_t cfg0 = make_cfg(0U, 0U, s_stack0);
    xRTOS_Task_Config_t cfg1 = make_cfg(1U, 1U, s_stack1);
    (void)xRTOS_Task_Create(&s_task0, &cfg0);
    (void)xRTOS_Task_Create(&s_task1, &cfg1);

    // 2 ticks with task 0 running, then 1 tick with task 1 running.
    s_kernel.scheduler.current_task_id = 0U;
    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);

    s_kernel.scheduler.current_task_id = 1U;
    xRTOS_Tick_Increment_From_ISR(&yield);

    xRTOS_Task_CPU_Stats_t out0, out1;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out0);
    (void)xRTOS_Task_Get_CPU_Stats(1U, &out1);

    TEST_ASSERT_EQUAL_UINT32(2U, out0.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(1U, out1.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(3U, out0.total_ticks);
    TEST_ASSERT_EQUAL_UINT32(3U, out1.total_ticks);
}

void test_total_ticks_is_shared_across_tasks(void)
{
    xRTOS_Task_Config_t cfg0 = make_cfg(0U, 0U, s_stack0);
    xRTOS_Task_Config_t cfg1 = make_cfg(1U, 1U, s_stack1);
    (void)xRTOS_Task_Create(&s_task0, &cfg0);
    (void)xRTOS_Task_Create(&s_task1, &cfg1);

    s_kernel.scheduler.current_task_id = 0U;
    bool yield = false;
    for (uint32_t i = 0U; i < 10U; i++)
    {
        xRTOS_Tick_Increment_From_ISR(&yield);
    }

    xRTOS_Task_CPU_Stats_t out0, out1;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out0);
    (void)xRTOS_Task_Get_CPU_Stats(1U, &out1);

    TEST_ASSERT_EQUAL_UINT32(10U, out0.total_ticks);
    TEST_ASSERT_EQUAL_UINT32(10U, out1.total_ticks);
}

void test_reset_all_zeroes_counters(void)
{
    xRTOS_Task_Config_t cfg0 = make_cfg(0U, 0U, s_stack0);
    xRTOS_Task_Config_t cfg1 = make_cfg(1U, 1U, s_stack1);
    (void)xRTOS_Task_Create(&s_task0, &cfg0);
    (void)xRTOS_Task_Create(&s_task1, &cfg1);

    s_kernel.scheduler.current_task_id = 0U;
    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);

    xRTOS_CPU_Stats_Reset_All();

    xRTOS_Task_CPU_Stats_t out0, out1;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out0);
    (void)xRTOS_Task_Get_CPU_Stats(1U, &out1);

    TEST_ASSERT_EQUAL_UINT32(0U, out0.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out1.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out0.total_ticks);
    TEST_ASSERT_EQUAL_UINT32(0U, out1.total_ticks);
}

void test_reset_all_then_accumulate_again(void)
{
    xRTOS_Task_Config_t cfg = make_cfg(0U, 0U, s_stack0);
    (void)xRTOS_Task_Create(&s_task0, &cfg);

    s_kernel.scheduler.current_task_id = 0U;
    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_CPU_Stats_Reset_All();
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);

    xRTOS_Task_CPU_Stats_t out;
    (void)xRTOS_Task_Get_CPU_Stats(0U, &out);
    TEST_ASSERT_EQUAL_UINT32(2U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(2U, out.total_ticks);
}

void test_get_cpu_stats_invalid_task_id(void)
{
    xRTOS_Task_CPU_Stats_t out;
    xRETURN_t ret = xRTOS_Task_Get_CPU_Stats(0U, &out); // task 0 not created
    TEST_ASSERT_EQUAL_INT(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out.total_ticks);
}

void test_get_cpu_stats_out_of_range_task_id(void)
{
    xRTOS_Task_CPU_Stats_t out;
    xRETURN_t ret = xRTOS_Task_Get_CPU_Stats(xRTOS_CONFIG_MAX_TASKS, &out);
    TEST_ASSERT_EQUAL_INT(xRETURN_xERR_xRTOS_INVALID_ARGUMENT, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, out.ticks_running);
    TEST_ASSERT_EQUAL_UINT32(0U, out.total_ticks);
}

#endif // xRTOS_CONFIG_CPU_STATS_ENABLE

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_get_cpu_stats_returns_invalid_state_when_disabled);
    RUN_TEST(test_reset_all_is_noop_when_disabled);

#if xRTOS_CONFIG_CPU_STATS_ENABLE
    RUN_TEST(test_ticks_running_starts_at_zero);
    RUN_TEST(test_tick_increments_current_task_counter);
    RUN_TEST(test_only_current_task_counter_increments);
    RUN_TEST(test_total_ticks_is_shared_across_tasks);
    RUN_TEST(test_reset_all_zeroes_counters);
    RUN_TEST(test_reset_all_then_accumulate_again);
    RUN_TEST(test_get_cpu_stats_invalid_task_id);
    RUN_TEST(test_get_cpu_stats_out_of_range_task_id);
#endif

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
