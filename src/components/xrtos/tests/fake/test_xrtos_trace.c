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

// @file test_xrtos_trace.c
// @brief Host tests for xRTOS xTRACE v2 integration.
//
// Tests verify that xRTOS_Kernel_Trace_Init stores the trace context and that
// scheduler operations emit the expected LEB128 events into the byte buffer.
//

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_mutex.h"
#include "xrtos_return.h"
#include "xrtos_scheduler.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_timer.h"
#include "xrtos_trace.h"
#include "xrtos_port_fake.h"
#include "xtrace.h"

// FIXTURES ////////////////////////////////////////////////////////////////////

static xRTOS_Kernel_Context_t s_kernel_ctx;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[64U];
static xRTOS_Task_Context_t s_task_ctx[4];
static uint32_t s_stack[4][64U];
static xRTOS_Mutex_Context_t s_mutex;
static xRTOS_Timer_Context_t s_timer;

// Trace fixtures - 512-byte LEB128 byte buffer.
static xTRACE_Context_t s_trace_ctx;
static uint8_t s_trace_buf[512U];

static xTRACE_Time_t stub_timestamp(void *ctx)
{
    (void)ctx;
    return 0U;
}

static void dummy_entry(void *arg)
{
    (void)arg;
}

static void timer_callback(void *arg)
{
    (void)arg;
}

static xRETURN_t register_idle(void)
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

static void attach_trace(void)
{
    xTRACE_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_trace_buf;
    cfg.capacity_bytes = (uint32_t)sizeof(s_trace_buf);
    cfg.timestamp_fn = stub_timestamp;
    cfg.timestamp_ctx = NULL;
    cfg.timestamp_hz = 1U;
    cfg.is_enabled = true;

    (void)xTRACE_Init(&s_trace_ctx, &cfg, NULL, NULL);
    (void)xRTOS_Kernel_Trace_Init(&s_trace_ctx);
}

void setUp(void)
{
    (void)xRTOS_Kernel_Init(&s_kernel_ctx, &xrtos_fake_port_ops);
    memset(s_trace_buf, 0, sizeof(s_trace_buf));
    memset(&s_trace_ctx, 0, sizeof(s_trace_ctx));
}

void tearDown(void)
{
}

// LEB128 SCAN HELPERS /////////////////////////////////////////////////////////

#if xTRACE_ENABLE

// Decode one LEB128 uint32 from buf at *pos; advances *pos.
static uint32_t leb128_read(const uint8_t *buf, uint32_t *pos)
{
    uint32_t value = 0U;
    uint32_t shift = 0U;
    while (true)
    {
        uint8_t byte = buf[(*pos)++];
        value |= ((uint32_t)(byte & 0x7FU)) << shift;
        shift += 7U;
        if ((byte & 0x80U) == 0U)
        {
            break;
        }
    }
    return value;
}

// Param count per xRTOS event ID.
static uint32_t event_param_count(uint32_t id)
{
    switch (id)
    {
    // 2-param events
    case xRTOS_TRACE_CODE_TASK_CREATE:
    case xRTOS_TRACE_CODE_TASK_SWITCH:
    case xRTOS_TRACE_CODE_TASK_BLOCK:
    case xRTOS_TRACE_CODE_TASK_PRIO:
    case xRTOS_TRACE_CODE_MUTEX_HANDOFF:
        return 2U;
    case 0x00U: // GAP: 1 param (dropped_count)
    case 0x01U: // BOOT: 1 param (timestamp_hz)
    default:
        return 1U; // all other xRTOS events have 1 param
    }
}

// Reset stream positions so the next Emit overwrites from byte 0.
// last_timestamp and is_gap_pending stay as-is so deltas remain valid.
static void reset_trace_positions(void)
{
    s_trace_ctx.write_pos = 0U;
    s_trace_ctx.read_pos = 0U;
}

// Scan the stream from [start..write_pos) and return true when event_id is
// found with param at param_idx equal to expected.  Also outputs both params.
static bool trace_find_event(uint32_t target_id, uint32_t param_idx, uint32_t expected, uint32_t *out_p0, uint32_t *out_p1)
{
    uint32_t pos = 0U;
    while (pos < s_trace_ctx.write_pos)
    {
        uint32_t record_len = leb128_read(s_trace_buf, &pos);
        (void)record_len;
        uint32_t ev = leb128_read(s_trace_buf, &pos);
        uint32_t delta = leb128_read(s_trace_buf, &pos);
        (void)delta;
        uint32_t n = event_param_count(ev);
        uint32_t p0 = 0U;
        uint32_t p1 = 0U;
        if (n >= 1U)
        {
            p0 = leb128_read(s_trace_buf, &pos);
        }
        if (n >= 2U)
        {
            p1 = leb128_read(s_trace_buf, &pos);
        }

        if (ev == target_id)
        {
            uint32_t match_val = (param_idx == 0U) ? p0 : p1;
            if (out_p0 != NULL)
            {
                *out_p0 = p0;
            }
            if (out_p1 != NULL)
            {
                *out_p1 = p1;
            }
            if (match_val == expected)
            {
                return true;
            }
        }
    }
    return false;
}

static bool trace_contains_event_p0(uint32_t id, uint32_t expected_p0)
{
    return trace_find_event(id, 0U, expected_p0, NULL, NULL);
}

static bool trace_contains_event_p1(uint32_t id, uint32_t expected_p1)
{
    return trace_find_event(id, 1U, expected_p1, NULL, NULL);
}

static bool trace_has_event(uint32_t id)
{
    uint32_t pos = 0U;
    while (pos < s_trace_ctx.write_pos)
    {
        uint32_t record_len = leb128_read(s_trace_buf, &pos);
        (void)record_len;
        uint32_t ev = leb128_read(s_trace_buf, &pos);
        uint32_t delta = leb128_read(s_trace_buf, &pos);
        (void)delta;
        uint32_t n = event_param_count(ev);
        for (uint32_t i = 0U; i < n; i++)
        {
            (void)leb128_read(s_trace_buf, &pos);
        }
        if (ev == id)
        {
            return true;
        }
    }
    return false;
}

#endif // xTRACE_ENABLE

// TESTS: xRTOS_Kernel_Trace_Init //////////////////////////////////////////////

void test_trace_init_returns_ok(void)
{
    xTRACE_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_trace_buf;
    cfg.capacity_bytes = (uint32_t)sizeof(s_trace_buf);
    cfg.timestamp_fn = stub_timestamp;
    cfg.timestamp_hz = 1U;
    cfg.is_enabled = true;

    (void)xTRACE_Init(&s_trace_ctx, &cfg, NULL, NULL);
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Kernel_Trace_Init(&s_trace_ctx));
}

void test_trace_init_null_returns_null_pointer_error(void)
{
#if xTRACE_ENABLE
    TEST_ASSERT_NOT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Kernel_Trace_Init(NULL));
#else
    TEST_ASSERT_EQUAL(xRETURN_xRTOS_OK, xRTOS_Kernel_Trace_Init(NULL));
#endif
}

void test_trace_init_stores_ctx_in_kernel(void)
{
#if xTRACE_ENABLE
    attach_trace();
    TEST_ASSERT_EQUAL_PTR(&s_trace_ctx, s_kernel_ctx.trace_ctx);
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - trace_ctx field not present");
#endif
}

// TESTS: trace emission ///////////////////////////////////////////////////////

#if xTRACE_ENABLE

void test_trace_task_create_emits_event_with_task_id(void)
{
    attach_trace();
    reset_trace_positions();

    (void)register_task(0U, 3U, 3U);

    // TASK_CREATE params: (task_id=3, priority=3)
    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TASK_CREATE, 3U));
}

void test_trace_task_create_second_param_is_priority(void)
{
    attach_trace();
    reset_trace_positions();

    (void)register_task(0U, 3U, 7U); // task_id=3, priority=7

    TEST_ASSERT_TRUE(trace_contains_event_p1(xRTOS_TRACE_CODE_TASK_CREATE, 7U));
}

void test_trace_kernel_start_emits_kernel_start_event(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_idle();
    attach_trace();

    (void)xRTOS_Kernel_Start();

    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_KERNEL_START, 3U));
}

void test_trace_block_current_emits_task_block_event(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();
    reset_trace_positions();

    (void)xRTOS_Scheduler_Block_Current(NULL);

    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TASK_BLOCK, 3U));
    TEST_ASSERT_TRUE(trace_contains_event_p1(xRTOS_TRACE_CODE_TASK_BLOCK, 0U));
}

void test_trace_unblock_emits_task_ready_event(void)
{
    (void)register_task(0U, 5U, 5U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    reset_trace_positions();

    (void)xRTOS_Scheduler_Unblock(5U, xRETURN_xRTOS_OK);

    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TASK_READY, 5U));
}

void test_trace_switch_emits_task_switch_event(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Select_Next();
    reset_trace_positions();

    xRTOS_Scheduler_Switch();

    TEST_ASSERT_TRUE(trace_has_event(xRTOS_TRACE_CODE_TASK_SWITCH));
}

void test_trace_switch_second_param_is_next_task_id(void)
{
    (void)register_task(0U, 2U, 2U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();

    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Select_Next();
    reset_trace_positions();

    xRTOS_Scheduler_Switch();

    // After blocking task 2, next is idle (task_id = xRTOS_IDLE_TASK_ID).
    TEST_ASSERT_TRUE(trace_contains_event_p1(xRTOS_TRACE_CODE_TASK_SWITCH, xRTOS_IDLE_TASK_ID));
}

void test_trace_switch_emits_task_ready_for_preempted_running_task(void)
{
    (void)register_task(0U, 1U, 1U);
    (void)register_task(1U, 9U, 9U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();

    s_kernel_ctx.scheduler.next_task_id = 9U;
    s_kernel_ctx.scheduler.next_priority = 9U;
    reset_trace_positions();

    xRTOS_Scheduler_Switch();

    TEST_ASSERT_TRUE(trace_has_event(xRTOS_TRACE_CODE_TASK_READY));
}

void test_trace_priority_change_emits_task_prio_event(void)
{
    (void)register_task(0U, 7U, 7U);
    attach_trace();
    reset_trace_positions();

    (void)xRTOS_Scheduler_Set_Effective_Priority(7U, 2U);

    // TASK_PRIO params: (task_id=7, new_priority=2)
    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TASK_PRIO, 7U));
    TEST_ASSERT_TRUE(trace_contains_event_p1(xRTOS_TRACE_CODE_TASK_PRIO, 2U));
}

void test_trace_timeout_emits_task_timeout_event(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();
    attach_trace();

    (void)xRTOS_Task_Delay(2U);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();
    reset_trace_positions();

    bool yield = false;
    xRTOS_Tick_Increment_From_ISR(&yield);
    xRTOS_Tick_Increment_From_ISR(&yield);

    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TASK_TIMEOUT, 3U));
}

void test_trace_timer_start_stop_emit_events(void)
{
    xRTOS_Timer_Config_t cfg;
    cfg.timer_id = 2U;
    cfg.callback = timer_callback;
    cfg.callback_arg = NULL;
    cfg.period_ticks = 10U;
    cfg.is_periodic = false;
    cfg.name = NULL;

    (void)xRTOS_Timer_Init(&s_timer, &cfg);
    attach_trace();
    reset_trace_positions();

    (void)xRTOS_Timer_Start(&s_timer);
    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TIMER_START, 2U));

    reset_trace_positions();
    (void)xRTOS_Timer_Stop(&s_timer);
    TEST_ASSERT_TRUE(trace_contains_event_p0(xRTOS_TRACE_CODE_TIMER_STOP, 2U));
}

void test_trace_mutex_handoff_second_param_is_new_owner(void)
{
    (void)xRTOS_Mutex_Init(&s_mutex, NULL);
    (void)register_task(0U, 10U, 10U);
    (void)register_task(1U, 5U, 5U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    s_kernel_ctx.scheduler.current_task_id = 10U;
    s_kernel_ctx.scheduler.current_priority = 10U;
    s_task_ctx[0U].state = xRTOS_TASK_STATE_RUNNING;
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_NO_WAIT);

    s_kernel_ctx.scheduler.current_task_id = 5U;
    s_kernel_ctx.scheduler.current_priority = 5U;
    s_task_ctx[1U].state = xRTOS_TASK_STATE_RUNNING;
    (void)xRTOS_Mutex_Lock(&s_mutex, xRTOS_WAIT_FOREVER);

    attach_trace();
    reset_trace_positions();

    s_kernel_ctx.scheduler.current_task_id = 10U;
    s_kernel_ctx.scheduler.current_priority = s_task_ctx[0U].effective_priority;
    s_task_ctx[0U].state = xRTOS_TASK_STATE_RUNNING;
    (void)xRTOS_Mutex_Unlock(&s_mutex);

    // MUTEX_HANDOFF params: (prev_owner, new_owner).  new_owner is task_id 5.
    TEST_ASSERT_TRUE(trace_contains_event_p1(xRTOS_TRACE_CODE_MUTEX_HANDOFF, 5U));
}

void test_no_events_emitted_without_trace_init(void)
{
    (void)register_task(0U, 3U, 3U);
    (void)register_idle();
    (void)xRTOS_Kernel_Start();

    // No trace attached - these must not crash.
    (void)xRTOS_Scheduler_Block_Current(NULL);
    (void)xRTOS_Scheduler_Select_Next();
    xRTOS_Scheduler_Switch();

    TEST_PASS();
}

#endif // xTRACE_ENABLE

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_trace_init_returns_ok);
    RUN_TEST(test_trace_init_null_returns_null_pointer_error);
    RUN_TEST(test_trace_init_stores_ctx_in_kernel);

#if xTRACE_ENABLE
    RUN_TEST(test_trace_task_create_emits_event_with_task_id);
    RUN_TEST(test_trace_task_create_second_param_is_priority);
    RUN_TEST(test_trace_kernel_start_emits_kernel_start_event);
    RUN_TEST(test_trace_block_current_emits_task_block_event);
    RUN_TEST(test_trace_unblock_emits_task_ready_event);
    RUN_TEST(test_trace_switch_emits_task_switch_event);
    RUN_TEST(test_trace_switch_second_param_is_next_task_id);
    RUN_TEST(test_trace_switch_emits_task_ready_for_preempted_running_task);
    RUN_TEST(test_trace_priority_change_emits_task_prio_event);
    RUN_TEST(test_trace_timeout_emits_task_timeout_event);
    RUN_TEST(test_trace_timer_start_stop_emit_events);
    RUN_TEST(test_trace_mutex_handoff_second_param_is_new_owner);
    RUN_TEST(test_no_events_emitted_without_trace_init);
#endif

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
