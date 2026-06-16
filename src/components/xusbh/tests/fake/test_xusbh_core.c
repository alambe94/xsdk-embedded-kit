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

// @file test_xusbh_core.c
// @brief Host tests for xUSBH lifecycle API validation and state tracking.

#include <string.h>

#include "unity.h"

#include "xusbh_core.h"
#include "xusbh_trace.h"
#include "test_xusbh_helpers.h"
#include "xtrace.h"

#if xTRACE_ENABLE
static xTRACE_Context_t s_trace_ctx;
static uint8_t s_trace_buf[256U];

static xTRACE_Time_t trace_timestamp(void *ctx)
{
    (void)ctx;
    return 0U;
}

static void trace_context_init(void)
{
    xTRACE_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_trace_buf;
    cfg.capacity_bytes = (uint32_t)sizeof(s_trace_buf);
    cfg.timestamp_fn = trace_timestamp;
    cfg.timestamp_ctx = NULL;
    cfg.timestamp_hz = 1U;
    cfg.is_enabled = true;

    memset(s_trace_buf, 0, sizeof(s_trace_buf));
    memset(&s_trace_ctx, 0, sizeof(s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xTRACE_Init(&s_trace_ctx, &cfg, NULL, NULL));
}

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

static void trace_positions_reset(void)
{
    s_trace_ctx.write_pos = 0U;
    s_trace_ctx.read_pos = 0U;
}

static bool trace_contains_event_arg(uint32_t target_id, uint32_t expected_arg)
{
    uint32_t pos = 0U;

    while (pos < s_trace_ctx.write_pos)
    {
        uint32_t record_len = leb128_read(s_trace_buf, &pos);
        uint32_t record_end = pos + record_len;
        uint32_t event_id = leb128_read(s_trace_buf, &pos);
        uint32_t delta = leb128_read(s_trace_buf, &pos);
        uint32_t arg = 0U;
        (void)delta;

        if (pos < record_end)
        {
            arg = leb128_read(s_trace_buf, &pos);
        }
        pos = record_end;

        if ((event_id == target_id) && (arg == expected_arg))
        {
            return true;
        }
    }

    return false;
}
#endif

void setUp(void)
{
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_xusbh_init_rejects_null_args(void)
{
    xUSBH_Context_t host_ctx;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Init(NULL, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Init(&host_ctx, NULL));
}

void test_xusbh_init_rejects_invalid_root_port_count(void)
{
    xUSBH_Context_t host_ctx;
    xUSBH_Init_Config_t config = {
        .root_port_count = 0U,
    };

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Init(&host_ctx, &config));

    config.root_port_count = (uint8_t)(xUSBH_MAX_ROOT_PORTS + 1U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Init(&host_ctx, &config));
}

void test_xusbh_init_sets_context_defaults(void)
{
    xUSBH_Context_t host_ctx;
    xUSBH_Lifecycle_State_t state = xUSBH_LIFECYCLE_CREATED;
    bool is_started = true;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xUSBH_LIFECYCLE_INITIALIZED, host_ctx.lifecycle_state);
    TEST_ASSERT_EQUAL_UINT8(1U, host_ctx.root_port_count);
    TEST_ASSERT_TRUE(host_ctx.is_initialized);
    TEST_ASSERT_FALSE(host_ctx.is_started);
    TEST_ASSERT_NULL(host_ctx.hcd_ops);
    TEST_ASSERT_NULL(host_ctx.hcd_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Get_Lifecycle_State(&host_ctx, &state));
    TEST_ASSERT_EQUAL(xUSBH_LIFECYCLE_INITIALIZED, state);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Is_Started(&host_ctx, &is_started));
    TEST_ASSERT_FALSE(is_started);
}

void test_xusbh_start_requires_initialized_context(void)
{
    xUSBH_Context_t host_ctx = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Start(NULL, &valid_start_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Start(&host_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Start(&host_ctx, &valid_start_config));
}

void test_xusbh_start_rejects_missing_or_incomplete_hcd_ops(void)
{
    xUSBH_Context_t host_ctx;
    xUSBH_Start_Config_t config = valid_start_config;
    xUSBH_HCD_Ops_t incomplete_ops = fake_hcd_ops;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));

    config.hcd_ops = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_Start(&host_ctx, &config));

    incomplete_ops.port_power = NULL;
    config = valid_start_config;
    config.hcd_ops = &incomplete_ops;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_INCOMPLETE_OPS, xUSBH_Start(&host_ctx, &config));
    TEST_ASSERT_FALSE(host_ctx.is_started);
}

void test_xusbh_start_sets_started_state(void)
{
    xUSBH_Context_t host_ctx;
    bool is_started = false;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_EQUAL(xUSBH_LIFECYCLE_STARTED, host_ctx.lifecycle_state);
    TEST_ASSERT_TRUE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_PTR(&fake_hcd_ops, host_ctx.hcd_ops);
    TEST_ASSERT_EQUAL_PTR(&g_fake_hcd, host_ctx.hcd_ctx);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.init_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.start_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.enable_interrupts_count);
    TEST_ASSERT_EQUAL_PTR(&host_ctx, g_fake_hcd.last_host_ctx);
    TEST_ASSERT_NOT_NULL(g_fake_hcd.last_callback);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Is_Started(&host_ctx, &is_started));
    TEST_ASSERT_TRUE(is_started);
}

void test_xusbh_trace_init_attaches_and_detaches_context(void)
{
#if xTRACE_ENABLE
    xUSBH_Context_t host_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    trace_context_init();

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Trace_Init(NULL, &s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Trace_Init(&host_ctx, &s_trace_ctx));
    TEST_ASSERT_EQUAL_PTR(&s_trace_ctx, host_ctx.trace_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Trace_Init(&host_ctx, NULL));
    TEST_ASSERT_NULL(host_ctx.trace_ctx);
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBH trace context storage is compiled out");
#endif
}

void test_xusbh_trace_records_port_and_transfer_events(void)
{
#if xTRACE_ENABLE
    xUSBH_Context_t host_ctx;
    xUSBH_Transfer_t *transfer = NULL;
    xUSBH_HCD_Event_t port_event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = 0U,
        .port_event = xUSBH_HCD_PORT_EVENT_CONNECTED,
    };
    xUSBH_HCD_Event_t transfer_event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    trace_context_init();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Trace_Init(&host_ctx, &s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    trace_positions_reset();

    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &port_event);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBH_TRACE_CODE_PORT_CONNECT, 0U));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&host_ctx, &transfer));
    transfer->device_address = 1U;
    transfer->endpoint_address = 0x81U;
    transfer->endpoint_type = USB_ENDP_TYPE_INTR;
    transfer->length = 16U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Submit(&host_ctx, transfer));
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBH_TRACE_CODE_TRANSFER_SUBMIT, 0x81U));

    transfer_event.transfer = transfer;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &transfer_event);
    TEST_ASSERT_TRUE(trace_contains_event_arg(xUSBH_TRACE_CODE_TRANSFER_COMPLETE, 0x81U));
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBH trace emission is compiled out");
#endif
}

void test_xusbh_trace_detach_stops_event_emission_without_changing_behavior(void)
{
#if xTRACE_ENABLE
    xUSBH_Context_t host_ctx;
    xUSBH_HCD_Event_t port_event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = 0U,
        .port_event = xUSBH_HCD_PORT_EVENT_CONNECTED,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    trace_context_init();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Trace_Init(&host_ctx, &s_trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Trace_Init(&host_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    trace_positions_reset();

    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &port_event);

    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONNECTED, host_ctx.root_ports[0].state);
    TEST_ASSERT_FALSE(trace_contains_event_arg(xUSBH_TRACE_CODE_PORT_CONNECT, 0U));
#else
    TEST_IGNORE_MESSAGE("xTRACE_ENABLE is 0 - xUSBH trace emission is compiled out");
#endif
}

void test_xusbh_start_propagates_hcd_init_failure(void)
{
    xUSBH_Context_t host_ctx;

    g_fake_hcd.init_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_FALSE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.init_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.start_count);
}

void test_xusbh_start_propagates_hcd_start_failure_and_deinits(void)
{
    xUSBH_Context_t host_ctx;

    g_fake_hcd.start_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_FALSE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.init_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.start_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.enable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.deinit_count);
}

void test_xusbh_start_propagates_hcd_enable_interrupts_failure_and_cleans_up(void)
{
    xUSBH_Context_t host_ctx;

    g_fake_hcd.enable_interrupts_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_FALSE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.init_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.start_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.enable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.deinit_count);
}

void test_xusbh_start_rejects_double_start(void)
{
    xUSBH_Context_t host_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_ALREADY_STARTED, xUSBH_Start(&host_ctx, &valid_start_config));
}

void test_xusbh_process_requires_started_context(void)
{
    xUSBH_Context_t host_ctx = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Process(NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Process(&host_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_STARTED, xUSBH_Process(&host_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&host_ctx));
}

void test_xusbh_stop_requires_started_context(void)
{
    xUSBH_Context_t host_ctx = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Stop(NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Stop(&host_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&host_ctx, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_STARTED, xUSBH_Stop(&host_ctx));
}

void test_xusbh_stop_clears_started_state_and_allows_restart(void)
{
    xUSBH_Context_t host_ctx;
    bool is_started = true;

    init_and_start_host(&host_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Stop(&host_ctx));
    TEST_ASSERT_EQUAL(xUSBH_LIFECYCLE_STOPPED, host_ctx.lifecycle_state);
    TEST_ASSERT_FALSE(host_ctx.is_started);
    TEST_ASSERT_NULL(host_ctx.hcd_ops);
    TEST_ASSERT_NULL(host_ctx.hcd_ctx);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.disable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.deinit_count);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Is_Started(&host_ctx, &is_started));
    TEST_ASSERT_FALSE(is_started);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&host_ctx, &valid_start_config));
    TEST_ASSERT_EQUAL(xUSBH_LIFECYCLE_STARTED, host_ctx.lifecycle_state);
}

void test_xusbh_stop_propagates_hcd_disable_interrupts_failure(void)
{
    xUSBH_Context_t host_ctx;

    init_and_start_host(&host_ctx);
    g_fake_hcd.disable_interrupts_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Stop(&host_ctx));
    TEST_ASSERT_TRUE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.disable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.deinit_count);
}

void test_xusbh_stop_propagates_hcd_stop_failure(void)
{
    xUSBH_Context_t host_ctx;

    init_and_start_host(&host_ctx);
    g_fake_hcd.stop_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Stop(&host_ctx));
    TEST_ASSERT_TRUE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.disable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.deinit_count);
}

void test_xusbh_stop_propagates_hcd_deinit_failure(void)
{
    xUSBH_Context_t host_ctx;

    init_and_start_host(&host_ctx);
    g_fake_hcd.deinit_return = xRETURN_xERR_xUSBH_INVALID_CONFIGURATION;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_CONFIGURATION, xUSBH_Stop(&host_ctx));
    TEST_ASSERT_TRUE(host_ctx.is_started);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.disable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.deinit_count);
}

void test_xusbh_accessors_reject_null_args(void)
{
    xUSBH_Context_t host_ctx = {0};
    xUSBH_Lifecycle_State_t state = xUSBH_LIFECYCLE_CREATED;
    bool is_started = false;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Get_Lifecycle_State(NULL, &state));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Get_Lifecycle_State(&host_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Is_Started(NULL, &is_started));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Is_Started(&host_ctx, NULL));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_init_rejects_null_args);
    RUN_TEST(test_xusbh_init_rejects_invalid_root_port_count);
    RUN_TEST(test_xusbh_init_sets_context_defaults);
    RUN_TEST(test_xusbh_start_requires_initialized_context);
    RUN_TEST(test_xusbh_start_rejects_missing_or_incomplete_hcd_ops);
    RUN_TEST(test_xusbh_start_sets_started_state);
    RUN_TEST(test_xusbh_trace_init_attaches_and_detaches_context);
    RUN_TEST(test_xusbh_trace_records_port_and_transfer_events);
    RUN_TEST(test_xusbh_trace_detach_stops_event_emission_without_changing_behavior);
    RUN_TEST(test_xusbh_start_propagates_hcd_init_failure);
    RUN_TEST(test_xusbh_start_propagates_hcd_start_failure_and_deinits);
    RUN_TEST(test_xusbh_start_propagates_hcd_enable_interrupts_failure_and_cleans_up);
    RUN_TEST(test_xusbh_start_rejects_double_start);
    RUN_TEST(test_xusbh_process_requires_started_context);
    RUN_TEST(test_xusbh_stop_requires_started_context);
    RUN_TEST(test_xusbh_stop_clears_started_state_and_allows_restart);
    RUN_TEST(test_xusbh_stop_propagates_hcd_disable_interrupts_failure);
    RUN_TEST(test_xusbh_stop_propagates_hcd_stop_failure);
    RUN_TEST(test_xusbh_stop_propagates_hcd_deinit_failure);
    RUN_TEST(test_xusbh_accessors_reject_null_args);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
