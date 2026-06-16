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

// @file test_xrtos_footprint_report.c
// @brief Compiler-derived RAM footprint report for xRTOS public context types.

#include <stdio.h>
#include <stdint.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_event.h"
#include "xrtos_mutex.h"
#include "xrtos_queue.h"
#include "xrtos_scheduler.h"
#include "xrtos_sem.h"
#include "xrtos_task.h"
#include "xrtos_timer.h"

#ifndef xRTOS_FOOTPRINT_PROFILE
#define xRTOS_FOOTPRINT_PROFILE "default"
#endif

#define xRTOS_MEMBER_SIZE(type, member)         (sizeof(((type *)0)->member))
#define xRTOS_FOOTPRINT_MAX_TASK_CONTEXT_BYTES  ((sizeof(void *) >= 8U) ? 144U : 88U)
#define xRTOS_FOOTPRINT_MAX_QUEUE_CONTEXT_BYTES ((sizeof(void *) >= 8U) ? 48U : 36U)
#define xRTOS_FOOTPRINT_MAX_MUTEX_CONTEXT_BYTES ((sizeof(void *) >= 8U) ? 48U : 28U)
#define xRTOS_FOOTPRINT_MAX_EVENT_CONTEXT_BYTES ((sizeof(void *) >= 8U) ? 16U : 12U)
#define xRTOS_FOOTPRINT_MAX_SEM_CONTEXT_BYTES   ((sizeof(void *) >= 8U) ? 24U : 16U)

_Static_assert(sizeof(xRTOS_Task_Context_t) <= xRTOS_FOOTPRINT_MAX_TASK_CONTEXT_BYTES, "xRTOS_Task_Context_t footprint exceeded budget");
_Static_assert(sizeof(xRTOS_Queue_Context_t) <= xRTOS_FOOTPRINT_MAX_QUEUE_CONTEXT_BYTES, "xRTOS_Queue_Context_t footprint exceeded budget");
_Static_assert(sizeof(xRTOS_Mutex_Context_t) <= xRTOS_FOOTPRINT_MAX_MUTEX_CONTEXT_BYTES, "xRTOS_Mutex_Context_t footprint exceeded budget");
_Static_assert(sizeof(xRTOS_Event_Context_t) <= xRTOS_FOOTPRINT_MAX_EVENT_CONTEXT_BYTES, "xRTOS_Event_Context_t footprint exceeded budget");
_Static_assert(sizeof(xRTOS_Sem_Context_t) <= xRTOS_FOOTPRINT_MAX_SEM_CONTEXT_BYTES, "xRTOS_Sem_Context_t footprint exceeded budget");

void setUp(void)
{
}

void tearDown(void)
{
}

static void xrtos_footprint_report_row(FILE *stream, const char *kind, const char *name, size_t value)
{
    printf("xRTOS_FOOTPRINT,%s,%s,%s,%lu\n", xRTOS_FOOTPRINT_PROFILE, kind, name, (unsigned long)value);

    if (stream != NULL)
    {
        (void)fprintf(stream, "%s,%s,%s,%lu\n", xRTOS_FOOTPRINT_PROFILE, kind, name, (unsigned long)value);
    }
}

static void xrtos_footprint_report_config(FILE *stream)
{
    xrtos_footprint_report_row(stream, "value", "xRTOS_MAX_TASKS", xRTOS_MAX_TASKS);
    xrtos_footprint_report_row(stream, "value", "xRTOS_MAX_PRIORITIES", xRTOS_MAX_PRIORITIES);
    xrtos_footprint_report_row(stream, "value", "xRTOS_MAX_TIMERS", xRTOS_MAX_TIMERS);
    xrtos_footprint_report_row(stream, "value", "xRTOS_BITMAP_WIDTH", xRTOS_BITMAP_WIDTH);
    xrtos_footprint_report_row(stream, "value", "xRTOS_BITMAP_WORD_COUNT", xRTOS_BITMAP_WORD_COUNT);
    xrtos_footprint_report_row(stream, "bytes", "sizeof(void *)", sizeof(void *));
}

static void xrtos_footprint_report_contexts(FILE *stream)
{
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Bitmap_t)", sizeof(xRTOS_Bitmap_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Task_Block_Payload_t)", sizeof(xRTOS_Task_Block_Payload_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Task_Config_t)", sizeof(xRTOS_Task_Config_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Task_Context_t)", sizeof(xRTOS_Task_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Scheduler_Context_t)", sizeof(xRTOS_Scheduler_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Kernel_Context_t)", sizeof(xRTOS_Kernel_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Sem_Context_t)", sizeof(xRTOS_Sem_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Mutex_Context_t)", sizeof(xRTOS_Mutex_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Event_Context_t)", sizeof(xRTOS_Event_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Queue_Context_t)", sizeof(xRTOS_Queue_Context_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Timer_Config_t)", sizeof(xRTOS_Timer_Config_t));
    xrtos_footprint_report_row(stream, "bytes", "sizeof(xRTOS_Timer_Context_t)", sizeof(xRTOS_Timer_Context_t));
}

static void xrtos_footprint_report_scaling_tables(FILE *stream)
{
    xrtos_footprint_report_row(stream, "bytes", "xRTOS_Kernel_Context_t.task_table", xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, task_table));
    xrtos_footprint_report_row(stream, "bytes", "xRTOS_Kernel_Context_t.task_by_priority",
                               xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, task_by_priority));
    xrtos_footprint_report_row(stream, "bytes", "xRTOS_Kernel_Context_t.timer_table",
                               xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, timer_table));
    xrtos_footprint_report_row(stream, "bytes", "xRTOS_Scheduler_Context_t.ready_head",
                               xRTOS_MEMBER_SIZE(xRTOS_Scheduler_Context_t, ready_head));
    xrtos_footprint_report_row(stream, "bytes", "xRTOS_Scheduler_Context_t.ready_tail",
                               xRTOS_MEMBER_SIZE(xRTOS_Scheduler_Context_t, ready_tail));
}

void test_footprint_config_matches_bitmap_geometry(void)
{
    TEST_ASSERT_TRUE(xRTOS_BITMAP_WIDTH >= xRTOS_MAX_TASKS);
    TEST_ASSERT_TRUE(xRTOS_BITMAP_WIDTH >= xRTOS_MAX_PRIORITIES);
    TEST_ASSERT_TRUE(xRTOS_BITMAP_WIDTH >= xRTOS_MAX_TIMERS);
    TEST_ASSERT_EQUAL_UINT32(xRTOS_BITMAP_WORD_COUNT * sizeof(uint32_t), sizeof(xRTOS_Bitmap_t));
}

void test_footprint_tables_scale_with_specific_limits(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRTOS_MAX_TASKS * (uint32_t)sizeof(xRTOS_Task_Context_t *),
                             (uint32_t)xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, task_table));
    TEST_ASSERT_EQUAL_UINT32(xRTOS_MAX_PRIORITIES * (uint32_t)sizeof(xRTOS_Task_Context_t *),
                             (uint32_t)xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, task_by_priority));
    TEST_ASSERT_EQUAL_UINT32(xRTOS_MAX_TIMERS * (uint32_t)sizeof(struct xRTOS_Timer_Context_t *),
                             (uint32_t)xRTOS_MEMBER_SIZE(xRTOS_Kernel_Context_t, timer_table));
    TEST_ASSERT_EQUAL_UINT32(xRTOS_MAX_PRIORITIES * (uint32_t)sizeof(xRTOS_Task_Context_t *),
                             (uint32_t)xRTOS_MEMBER_SIZE(xRTOS_Scheduler_Context_t, ready_head));
    TEST_ASSERT_EQUAL_UINT32(xRTOS_MAX_PRIORITIES * (uint32_t)sizeof(xRTOS_Task_Context_t *),
                             (uint32_t)xRTOS_MEMBER_SIZE(xRTOS_Scheduler_Context_t, ready_tail));
}

void test_footprint_contexts_stay_within_budget(void)
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(xRTOS_FOOTPRINT_MAX_TASK_CONTEXT_BYTES, (uint32_t)sizeof(xRTOS_Task_Context_t));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(xRTOS_FOOTPRINT_MAX_QUEUE_CONTEXT_BYTES, (uint32_t)sizeof(xRTOS_Queue_Context_t));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(xRTOS_FOOTPRINT_MAX_MUTEX_CONTEXT_BYTES, (uint32_t)sizeof(xRTOS_Mutex_Context_t));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(xRTOS_FOOTPRINT_MAX_EVENT_CONTEXT_BYTES, (uint32_t)sizeof(xRTOS_Event_Context_t));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(xRTOS_FOOTPRINT_MAX_SEM_CONTEXT_BYTES, (uint32_t)sizeof(xRTOS_Sem_Context_t));
}

void test_footprint_report_is_written(void)
{
    FILE *stream = NULL;

#ifdef xRTOS_FOOTPRINT_REPORT_PATH
    stream = fopen(xRTOS_FOOTPRINT_REPORT_PATH, "w");
    TEST_ASSERT_NOT_NULL(stream);
#endif

    printf("xRTOS footprint report profile: %s\n", xRTOS_FOOTPRINT_PROFILE);
    // cppcheck-suppress knownConditionTrueFalse
    if (stream != NULL)
    {
        (void)fprintf(stream, "profile,kind,name,value\n");
    }

    xrtos_footprint_report_config(stream);
    xrtos_footprint_report_contexts(stream);
    xrtos_footprint_report_scaling_tables(stream);
    (void)fflush(stdout);

    // cppcheck-suppress knownConditionTrueFalse
    if (stream != NULL)
    {
        int close_res = fclose(stream);
        TEST_ASSERT_EQUAL_INT(0, close_res);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_footprint_config_matches_bitmap_geometry);
    RUN_TEST(test_footprint_tables_scale_with_specific_limits);
    RUN_TEST(test_footprint_contexts_stay_within_budget);
    RUN_TEST(test_footprint_report_is_written);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
