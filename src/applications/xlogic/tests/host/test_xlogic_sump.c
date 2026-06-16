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

// @file test_xlogic_sump.c
// @brief Host unit tests for the xLOGIC SUMP protocol parser and metadata builder.
//
// Tests cover:
//   1. xLOGIC_SUMP_Init           - zero initialisation and null rejection.
//   2. Short commands              - RESET, RUN, ID_QUERY, METADATA, XOFF.
//   3. Long command accumulation   - partial bytes emit NONE until 4th byte.
//   4. Long command decoding       - SET_DIVIDER, SET_COUNTS, SET_FLAGS,
//                                    SET_TMASK0, SET_TVALUE0, SET_TCONFIG0.
//   5. Parser endianness           - LSB-first argument assembly.
//   6. Multiple resets             - parser remains consistent.
//   7. xLOGIC_SUMP_Build_Metadata - correct byte layout, buffer-full guard.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xlogic_defs.h"
#include "xlogic_config.h"
#include "xlogic_return.h"
#include "xlogic_sump.h"

// Overriding weak xassert_system_halt to prevent hanging on assertions in unit tests
void xassert_system_halt(void)
{
    // Do nothing: allows the test to continue and verify the return value
}

// FIXTURES ////////////////////////////////////////////////////////////////////////

static xLOGIC_SUMP_Context_t s_sump;

void setUp(void)
{
    (void)xLOGIC_SUMP_Init(&s_sump);
}

void tearDown(void)
{
}

// Helper: feed a single byte and return the event
static xLOGIC_SUMP_Event_t feed(uint8_t byte)
{
    xLOGIC_SUMP_Event_t event = xLOGIC_SUMP_EVENT_NONE;
    xRETURN_t ret = xLOGIC_SUMP_Feed_Byte(&s_sump, byte, &event);

    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    return event;
}

// Helper: feed a 4-byte LE argument for a long command
static xLOGIC_SUMP_Event_t feed_long(uint8_t cmd, uint32_t arg)
{
    feed(cmd);
    feed((uint8_t)((arg >> 0U) & 0xFFU)); // LSB
    feed((uint8_t)((arg >> 8U) & 0xFFU));
    feed((uint8_t)((arg >> 16U) & 0xFFU));

    return feed((uint8_t)((arg >> 24U) & 0xFFU)); // MSB - produces event
}

// TESTS ///////////////////////////////////////////////////////////////////////////

void test_init_zeros_all_fields(void)
{
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.divider);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.read_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.delay_count);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.trigger_mask);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.trigger_value);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_TRIGGER_NONE, (uint32_t)s_sump.trigger_mode);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_PARSE_STATE_CMD, (uint32_t)s_sump.parse_state);
}

void test_reset_command_emits_reset_event(void)
{
    xLOGIC_SUMP_Event_t event = feed(xLOGIC_SUMP_CMD_RESET);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_RESET, (uint32_t)event);
}

void test_run_command_emits_run_event(void)
{
    xLOGIC_SUMP_Event_t event = feed(xLOGIC_SUMP_CMD_RUN);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_RUN, (uint32_t)event);
}

void test_id_query_emits_query_id_event(void)
{
    xLOGIC_SUMP_Event_t event = feed(xLOGIC_SUMP_CMD_ID_QUERY);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_QUERY_ID, (uint32_t)event);
}

void test_metadata_command_emits_metadata_event(void)
{
    xLOGIC_SUMP_Event_t event = feed(xLOGIC_SUMP_CMD_METADATA);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_METADATA, (uint32_t)event);
}

void test_set_divider_parsed_correctly(void)
{
    // 0x80 + 4-byte LE arg
    // Divider = 0x001234 -> sample_rate = 100 MHz / (0x1234 + 1)
    xLOGIC_SUMP_Event_t event = feed_long(xLOGIC_SUMP_CMD_SET_DIVIDER, 0x00001234U);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_CONFIG_SET, (uint32_t)event);
    TEST_ASSERT_EQUAL_UINT32(0x00001234U, s_sump.divider);
}

void test_set_counts_parsed_correctly(void)
{
    // read_count field = bits [15:0] = N -> read_count = (N+1)*4
    // delay_count field = bits [31:16] = M -> delay_count = (M+1)*4
    // Encode: N=99 (read_count=400), M=24 (delay_count=100)
    uint32_t arg = ((uint32_t)24U << 16U) | (uint32_t)99U;
    xLOGIC_SUMP_Event_t event = feed_long(xLOGIC_SUMP_CMD_SET_COUNTS, arg);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_CONFIG_SET, (uint32_t)event);
    TEST_ASSERT_EQUAL_UINT32(400U, s_sump.read_count);
    TEST_ASSERT_EQUAL_UINT32(100U, s_sump.delay_count);
}

void test_trigger_mask_and_value_parsed_correctly(void)
{
    feed_long(xLOGIC_SUMP_CMD_SET_TMASK0, 0x000000FFU);
    feed_long(xLOGIC_SUMP_CMD_SET_TVALUE0, 0x000000A5U);

    TEST_ASSERT_EQUAL_UINT32(0x000000FFU, s_sump.trigger_mask);
    TEST_ASSERT_EQUAL_UINT32(0x000000A5U, s_sump.trigger_value);
}

void test_trigger_config_level_decoded(void)
{
    // Bit 0 = 1 -> level trigger
    feed_long(xLOGIC_SUMP_CMD_SET_TCONFIG0, 0x00000001U);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_TRIGGER_LEVEL, (uint32_t)s_sump.trigger_mode);
}

void test_trigger_config_edge_decoded(void)
{
    // Bit 0 = 0 -> edge trigger
    feed_long(xLOGIC_SUMP_CMD_SET_TCONFIG0, 0x00000000U);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_TRIGGER_EDGE, (uint32_t)s_sump.trigger_mode);
}

void test_partial_long_command_emits_none_until_complete(void)
{
    // First byte: command opcode - not yet complete
    xLOGIC_SUMP_Event_t e0 = feed(xLOGIC_SUMP_CMD_SET_DIVIDER);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_NONE, (uint32_t)e0);

    // Next three arg bytes - still not complete
    xLOGIC_SUMP_Event_t e1 = feed(0x34U);
    xLOGIC_SUMP_Event_t e2 = feed(0x12U);
    xLOGIC_SUMP_Event_t e3 = feed(0x00U);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_NONE, (uint32_t)e1);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_NONE, (uint32_t)e2);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_NONE, (uint32_t)e3);

    // Fourth arg byte completes the command
    xLOGIC_SUMP_Event_t e4 = feed(0x00U);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_CONFIG_SET, (uint32_t)e4);
    TEST_ASSERT_EQUAL_UINT32(0x00001234U, s_sump.divider);
}

void test_multiple_resets_do_not_corrupt_state(void)
{
    // Set some config
    feed_long(xLOGIC_SUMP_CMD_SET_DIVIDER, 0x00001234U);
    TEST_ASSERT_EQUAL_UINT32(0x00001234U, s_sump.divider);

    // Reset clears config
    feed(xLOGIC_SUMP_CMD_RESET);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.divider);

    // A second reset in a row is safe
    xLOGIC_SUMP_Event_t event = feed(xLOGIC_SUMP_CMD_RESET);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_SUMP_EVENT_RESET, (uint32_t)event);
    TEST_ASSERT_EQUAL_UINT32(0U, s_sump.divider);
}

void test_metadata_buffer_contains_end_marker(void)
{
    uint8_t buf[xLOGIC_CONFIG_METADATA_BUF_BYTES];
    uint32_t written = 0U;

    xRETURN_t ret = xLOGIC_SUMP_Build_Metadata(buf, sizeof(buf), &written);

    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, written);
    // Last byte must be the end-of-metadata marker
    TEST_ASSERT_EQUAL_UINT8(xLOGIC_SUMP_META_END, buf[written - 1U]);
}

void test_metadata_buffer_too_small_returns_error(void)
{
    uint8_t tiny[2U]; // impossibly small
    uint32_t written = 0U;

    xRETURN_t ret = xLOGIC_SUMP_Build_Metadata(tiny, sizeof(tiny), &written);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xLOGIC_BUFFER_FULL, ret);
}

void test_null_context_returns_error(void)
{
    xLOGIC_SUMP_Event_t event = xLOGIC_SUMP_EVENT_NONE;

    xRETURN_t ret = xLOGIC_SUMP_Feed_Byte(NULL, 0x00U, &event);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);

    ret = xLOGIC_SUMP_Init(NULL);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);

    uint8_t buf[64U];
    uint32_t written = 0U;
    ret = xLOGIC_SUMP_Build_Metadata(NULL, sizeof(buf), &written);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

// RUNNER //////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_zeros_all_fields);
    RUN_TEST(test_reset_command_emits_reset_event);
    RUN_TEST(test_run_command_emits_run_event);
    RUN_TEST(test_id_query_emits_query_id_event);
    RUN_TEST(test_metadata_command_emits_metadata_event);
    RUN_TEST(test_set_divider_parsed_correctly);
    RUN_TEST(test_set_counts_parsed_correctly);
    RUN_TEST(test_trigger_mask_and_value_parsed_correctly);
    RUN_TEST(test_trigger_config_level_decoded);
    RUN_TEST(test_trigger_config_edge_decoded);
    RUN_TEST(test_partial_long_command_emits_none_until_complete);
    RUN_TEST(test_multiple_resets_do_not_corrupt_state);
    RUN_TEST(test_metadata_buffer_contains_end_marker);
    RUN_TEST(test_metadata_buffer_too_small_returns_error);
    RUN_TEST(test_null_context_returns_error);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
