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

// @file test_xfault.c
// @brief Host tests for xFAULT unwinding and text output.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xfault.h"

typedef struct Test_Output_Context_t
{
    char *buffer;
    size_t capacity;
    size_t length;
    size_t max_write;
    xRETURN_t return_value;
    uint32_t flush_count;
} Test_Output_Context_t;

void setUp(void)
{
}

void tearDown(void)
{
    (void)xFAULT_Fatal_Config_Set(NULL, 0U, 0U);
}

static xRETURN_t test_output_write(void *output_ctx, const uint8_t *buffer, size_t length, size_t *bytes_written)
{
    Test_Output_Context_t *ctx = (Test_Output_Context_t *)output_ctx;
    size_t write_length;

    if ((ctx == NULL) || (buffer == NULL) || (bytes_written == NULL))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    if (ctx->return_value != xRETURN_OK)
    {
        *bytes_written = 0U;
        return ctx->return_value;
    }

    write_length = length;

    if ((ctx->max_write != 0U) && (write_length > ctx->max_write))
    {
        write_length = ctx->max_write;
    }

    if (write_length > (ctx->capacity - ctx->length))
    {
        write_length = ctx->capacity - ctx->length;
    }

    if (write_length > 0U)
    {
        (void)memcpy(&ctx->buffer[ctx->length], buffer, write_length);
        ctx->length += write_length;
    }

    *bytes_written = write_length;

    return xRETURN_OK;
}

static xRETURN_t test_output_flush(void *output_ctx)
{
    Test_Output_Context_t *ctx = (Test_Output_Context_t *)output_ctx;

    if (ctx == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    ctx->flush_count++;

    return xRETURN_OK;
}

static xRETURN_t test_output_flush_failure(void *output_ctx)
{
    (void)output_ctx;

    return xRETURN_xERR_xFAULT_OUTPUT_FAILED;
}

static bool buffer_contains(const char *buffer, size_t length, const char *needle)
{
    size_t index;
    size_t needle_length;

    if ((buffer == NULL) || (needle == NULL))
    {
        return false;
    }

    needle_length = strlen(needle);

    if ((needle_length == 0U) || (needle_length > length))
    {
        return false;
    }

    for (index = 0U; index <= (length - needle_length); index++)
    {
        if (memcmp(&buffer[index], needle, needle_length) == 0)
        {
            return true;
        }
    }

    return false;
}

void test_context_init_zeroes_context(void)
{
    xFAULT_Context_t fault_ctx;

    (void)memset(&fault_ctx, 0xA5, sizeof(fault_ctx));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    TEST_ASSERT_EQUAL_UINT32(0U, fault_ctx.backtrace_count);
    TEST_ASSERT_EQUAL(xFAULT_EXCEPTION_TYPE_UNKNOWN, fault_ctx.exception_type);
    TEST_ASSERT_FALSE(fault_ctx.is_valid);
}

void test_context_init_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Context_Init(NULL));
}

void test_backtrace_capture_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Backtrace_Capture(NULL, 0x2000U, 0x1000U));
}

void test_backtrace_capture_extracts_frame_chain(void)
{
    xFAULT_Context_t fault_ctx;
    xFAULT_Address_t stack_words[6U];
    xFAULT_Address_t stack_limit;
    xFAULT_Address_t stack_base;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    // cppcheck-suppress legacyUninitvar
    stack_words[0U] = (xFAULT_Address_t)&stack_words[2U];
    stack_words[1U] = (xFAULT_Address_t)0x1001U;
    stack_words[2U] = (xFAULT_Address_t)&stack_words[4U];
    stack_words[3U] = (xFAULT_Address_t)0x1002U;
    stack_words[4U] = 0U;
    stack_words[5U] = (xFAULT_Address_t)0x1003U;

    stack_limit = (xFAULT_Address_t)&stack_words[0U];
    // cppcheck-suppress arrayIndexOutOfBounds
    stack_base = (xFAULT_Address_t)&stack_words[6U];
    fault_ctx.core.fp = (xFAULT_Address_t)&stack_words[0U];

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Backtrace_Capture(&fault_ctx, stack_base, stack_limit));
    TEST_ASSERT_EQUAL_UINT32(3U, fault_ctx.backtrace_count);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x1001U, (uint64_t)fault_ctx.backtrace[0U]);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x1002U, (uint64_t)fault_ctx.backtrace[1U]);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x1003U, (uint64_t)fault_ctx.backtrace[2U]);
    TEST_ASSERT_TRUE(fault_ctx.is_valid);
}

void test_backtrace_capture_rejects_invalid_stack_bounds(void)
{
    xFAULT_Context_t fault_ctx;
    // cppcheck-suppress constVariable
    xFAULT_Address_t stack_words[2U];

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_INVALID_ARGUMENT,
                      // cppcheck-suppress legacyUninitvar
                      xFAULT_Backtrace_Capture(&fault_ctx, (xFAULT_Address_t)&stack_words[0U], (xFAULT_Address_t)&stack_words[1U]));
}

void test_backtrace_capture_stops_on_out_of_range_frame(void)
{
    xFAULT_Context_t fault_ctx;
    // cppcheck-suppress constVariable
    xFAULT_Address_t stack_words[2U];

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    // cppcheck-suppress arrayIndexOutOfBounds
    // cppcheck-suppress legacyUninitvar
    fault_ctx.core.fp = (xFAULT_Address_t)&stack_words[2U];

    TEST_ASSERT_EQUAL(xRETURN_OK,
                      // cppcheck-suppress arrayIndexOutOfBounds
                      xFAULT_Backtrace_Capture(&fault_ctx, (xFAULT_Address_t)&stack_words[2U], (xFAULT_Address_t)&stack_words[0U]));
    TEST_ASSERT_EQUAL_UINT32(0U, fault_ctx.backtrace_count);
    TEST_ASSERT_TRUE(fault_ctx.is_valid);
}

void test_backtrace_capture_rejects_underflowing_stack_range(void)
{
    xFAULT_Context_t fault_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    fault_ctx.core.fp = 0U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Backtrace_Capture(&fault_ctx, (xFAULT_Address_t)sizeof(xFAULT_Address_t), 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, fault_ctx.backtrace_count);
    TEST_ASSERT_TRUE(fault_ctx.is_valid);
}

void test_backtrace_capture_respects_depth_limit(void)
{
    xFAULT_Context_t fault_ctx;
    xFAULT_Address_t stack_words[(xFAULT_MAX_BACKTRACE_DEPTH + 1U) * 2U];
    size_t index;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    for (index = 0U; index < (xFAULT_MAX_BACKTRACE_DEPTH + 1U); index++)
    {
        if (index < xFAULT_MAX_BACKTRACE_DEPTH)
        {
            stack_words[index * 2U] = (xFAULT_Address_t)&stack_words[(index + 1U) * 2U];
        }
        else
        {
            stack_words[index * 2U] = 0U;
        }

        stack_words[(index * 2U) + 1U] = (xFAULT_Address_t)(0x2000U + index);
    }

    fault_ctx.core.fp = (xFAULT_Address_t)&stack_words[0U];

    TEST_ASSERT_EQUAL(xRETURN_OK,
                      xFAULT_Backtrace_Capture(&fault_ctx, (xFAULT_Address_t)&stack_words[(size_t)(xFAULT_MAX_BACKTRACE_DEPTH + 1U) * 2U],
                                               (xFAULT_Address_t)&stack_words[0U]));
    TEST_ASSERT_EQUAL_UINT32(xFAULT_MAX_BACKTRACE_DEPTH, fault_ctx.backtrace_count);
}

void test_dump_text_rejects_null_arguments(void)
{
    xFAULT_Context_t fault_ctx;
    xFAULT_Config_t config;
    xFAULT_Output_t output = {.write = test_output_write, .flush = NULL};

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));

    config.output = &output;
    config.output_ctx = NULL;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Dump_Text(NULL, &config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Dump_Text(&fault_ctx, NULL));

    config.output = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Dump_Text(&fault_ctx, &config));

    output.write = NULL;
    config.output = &output;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Dump_Text(&fault_ctx, &config));
}

void test_dump_text_writes_parsable_backtrace_block(void)
{
    char buffer[2048U];
    Test_Output_Context_t output_ctx;
    xFAULT_Output_t output = {.write = test_output_write, .flush = test_output_flush};
    xFAULT_Config_t config;
    xFAULT_Context_t fault_ctx;

    (void)memset(buffer, 0, sizeof(buffer));
    output_ctx.buffer = buffer;
    output_ctx.capacity = sizeof(buffer);
    output_ctx.length = 0U;
    output_ctx.max_write = 0U;
    output_ctx.return_value = xRETURN_OK;
    output_ctx.flush_count = 0U;
    config.output = &output;
    config.output_ctx = &output_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    fault_ctx.exception_type = xFAULT_EXCEPTION_TYPE_DATA_ABORT;
    fault_ctx.core.pc = (xFAULT_Address_t)0x70001B2CU;
    fault_ctx.core.lr = (xFAULT_Address_t)0x70002F44U;
    fault_ctx.cp15.dfsr = 0x0000000DU;
    fault_ctx.cp15.dfar = 0xFFFFFFFCU;
    fault_ctx.backtrace[0U] = (xFAULT_Address_t)0x70001B2CU;
    fault_ctx.backtrace[1U] = (xFAULT_Address_t)0x70002F44U;
    fault_ctx.backtrace_count = 2U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Dump_Text(&fault_ctx, &config));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "[xFAULT_BT_START]\n"));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "70001B2C"));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "70002F44"));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "CPSR="));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "IFSR="));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "[xFAULT_BT_END]\n"));
    TEST_ASSERT_EQUAL_UINT32(1U, output_ctx.flush_count);
}

void test_dump_text_handles_partial_writes(void)
{
    char buffer[2048U];
    Test_Output_Context_t output_ctx;
    xFAULT_Output_t output = {.write = test_output_write, .flush = NULL};
    xFAULT_Config_t config;
    xFAULT_Context_t fault_ctx;

    (void)memset(buffer, 0, sizeof(buffer));
    output_ctx.buffer = buffer;
    output_ctx.capacity = sizeof(buffer);
    output_ctx.length = 0U;
    output_ctx.max_write = 3U;
    output_ctx.return_value = xRETURN_OK;
    output_ctx.flush_count = 0U;
    config.output = &output;
    config.output_ctx = &output_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    fault_ctx.backtrace[0U] = (xFAULT_Address_t)0x12345678U;
    fault_ctx.backtrace_count = 1U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Dump_Text(&fault_ctx, &config));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "12345678"));
}

void test_context_from_exception_frame_copies_target_registers(void)
{
    xFAULT_Context_t fault_ctx;
    xFAULT_Exception_Frame_t exception_frame;

    (void)memset(&exception_frame, 0, sizeof(exception_frame));
    exception_frame.r0 = 0x00000001U;
    exception_frame.r10 = 0x0000000AU;
    exception_frame.fp = 0x20000100U;
    exception_frame.ip = 0x0000000CU;
    exception_frame.sp = 0x20000200U;
    exception_frame.lr = 0x70001004U;
    exception_frame.pc = 0x70001000U;
    exception_frame.cpsr = 0x000000D7U;
    exception_frame.spsr = 0x0000001FU;
    exception_frame.exception_type = (uint32_t)xFAULT_EXCEPTION_TYPE_PREFETCH_ABORT;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_From_Exception_Frame(&fault_ctx, &exception_frame));
    TEST_ASSERT_EQUAL_UINT32(0x00000001U, fault_ctx.core.r0);
    TEST_ASSERT_EQUAL_UINT32(0x0000000AU, fault_ctx.core.r10);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x20000100U, (uint64_t)fault_ctx.core.fp);
    TEST_ASSERT_EQUAL_UINT32(0x0000000CU, fault_ctx.core.ip);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x20000200U, (uint64_t)fault_ctx.core.sp);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x70001004U, (uint64_t)fault_ctx.core.lr);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)0x70001000U, (uint64_t)fault_ctx.core.pc);
    TEST_ASSERT_EQUAL_UINT32(0x000000D7U, fault_ctx.core.cpsr);
    TEST_ASSERT_EQUAL_UINT32(0x0000001FU, fault_ctx.core.spsr);
    TEST_ASSERT_EQUAL(xFAULT_EXCEPTION_TYPE_PREFETCH_ABORT, fault_ctx.exception_type);
    TEST_ASSERT_TRUE(fault_ctx.is_valid);
}

void test_context_from_exception_frame_rejects_null_arguments(void)
{
    xFAULT_Context_t fault_ctx;
    xFAULT_Exception_Frame_t exception_frame;

    (void)memset(&exception_frame, 0, sizeof(exception_frame));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Context_From_Exception_Frame(NULL, &exception_frame));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Context_From_Exception_Frame(&fault_ctx, NULL));
}

void test_fatal_config_rejects_invalid_output_and_bounds(void)
{
    xFAULT_Output_t output = {.write = NULL, .flush = NULL};
    xFAULT_Config_t config = {.output = &output, .output_ctx = NULL, .halt = NULL, .halt_ctx = NULL};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Fatal_Config_Set(&config, 0U, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_INVALID_ARGUMENT, xFAULT_Fatal_Config_Set(NULL, 0x1000U, 0x1000U));
}

void test_fatal_process_captures_backtrace_and_dumps_output(void)
{
    char buffer[2048U];
    Test_Output_Context_t output_ctx;
    xFAULT_Output_t output = {.write = test_output_write, .flush = test_output_flush};
    xFAULT_Config_t config;
    xFAULT_Context_t fault_ctx;
    xFAULT_Address_t stack_words[4U];

    (void)memset(buffer, 0, sizeof(buffer));
    output_ctx.buffer = buffer;
    output_ctx.capacity = sizeof(buffer);
    output_ctx.length = 0U;
    output_ctx.max_write = 0U;
    output_ctx.return_value = xRETURN_OK;
    output_ctx.flush_count = 0U;
    config.output = &output;
    config.output_ctx = &output_ctx;
    config.halt = NULL;
    config.halt_ctx = NULL;

    // cppcheck-suppress legacyUninitvar
    stack_words[0U] = (xFAULT_Address_t)&stack_words[2U];
    stack_words[1U] = (xFAULT_Address_t)0x70001234U;
    stack_words[2U] = 0U;
    stack_words[3U] = (xFAULT_Address_t)0x70005678U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    fault_ctx.core.fp = (xFAULT_Address_t)&stack_words[0U];
    fault_ctx.exception_type = xFAULT_EXCEPTION_TYPE_DATA_ABORT;

    // cppcheck-suppress arrayIndexOutOfBounds
    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Fatal_Config_Set(&config, (xFAULT_Address_t)&stack_words[4U], (xFAULT_Address_t)&stack_words[0U]));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Fatal_Process(&fault_ctx));
    TEST_ASSERT_EQUAL_UINT32(2U, fault_ctx.backtrace_count);
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "[xFAULT_START]\n"));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "70001234"));
    TEST_ASSERT_TRUE(buffer_contains(buffer, output_ctx.length, "70005678"));
    TEST_ASSERT_EQUAL_UINT32(1U, output_ctx.flush_count);
}

void test_fatal_process_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Fatal_Process(NULL));
}

void test_dump_text_reports_output_failure(void)
{
    char buffer[64U];
    Test_Output_Context_t output_ctx;
    xFAULT_Output_t output = {.write = test_output_write, .flush = NULL};
    xFAULT_Config_t config;
    xFAULT_Context_t fault_ctx;

    output_ctx.buffer = buffer;
    output_ctx.capacity = sizeof(buffer);
    output_ctx.length = 0U;
    output_ctx.max_write = 0U;
    output_ctx.return_value = xRETURN_xERR_xFAULT_OUTPUT_FAILED;
    output_ctx.flush_count = 0U;
    config.output = &output;
    config.output_ctx = &output_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_OUTPUT_FAILED, xFAULT_Dump_Text(&fault_ctx, &config));
}

void test_dump_text_reports_flush_failure(void)
{
    char buffer[2048U];
    Test_Output_Context_t output_ctx;
    xFAULT_Output_t output = {.write = test_output_write, .flush = test_output_flush_failure};
    xFAULT_Config_t config;
    xFAULT_Context_t fault_ctx;

    output_ctx.buffer = buffer;
    output_ctx.capacity = sizeof(buffer);
    output_ctx.length = 0U;
    output_ctx.max_write = 0U;
    output_ctx.return_value = xRETURN_OK;
    output_ctx.flush_count = 0U;
    config.output = &output;
    config.output_ctx = &output_ctx;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFAULT_Context_Init(&fault_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_OUTPUT_FAILED, xFAULT_Dump_Text(&fault_ctx, &config));
}

void test_cp15_capture_returns_unsupported_on_host(void)
{
    xFAULT_CP15_Registers_t cp15;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_UNSUPPORTED_TARGET, xFAULT_Capture_CP15(&cp15));
    TEST_ASSERT_EQUAL_UINT32(0U, cp15.dfsr);
    TEST_ASSERT_EQUAL_UINT32(0U, cp15.dfar);
    TEST_ASSERT_EQUAL_UINT32(0U, cp15.ifsr);
    TEST_ASSERT_EQUAL_UINT32(0U, cp15.ifar);
}

void test_cp15_capture_rejects_null_pointer(void)
{
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFAULT_NULL_POINTER, xFAULT_Capture_CP15(NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_context_init_zeroes_context);
    RUN_TEST(test_context_init_rejects_null_context);
    RUN_TEST(test_backtrace_capture_rejects_null_context);
    RUN_TEST(test_backtrace_capture_extracts_frame_chain);
    RUN_TEST(test_backtrace_capture_rejects_invalid_stack_bounds);
    RUN_TEST(test_backtrace_capture_stops_on_out_of_range_frame);
    RUN_TEST(test_backtrace_capture_rejects_underflowing_stack_range);
    RUN_TEST(test_backtrace_capture_respects_depth_limit);
    RUN_TEST(test_dump_text_rejects_null_arguments);
    RUN_TEST(test_dump_text_writes_parsable_backtrace_block);
    RUN_TEST(test_dump_text_handles_partial_writes);
    RUN_TEST(test_dump_text_reports_output_failure);
    RUN_TEST(test_dump_text_reports_flush_failure);
    RUN_TEST(test_context_from_exception_frame_copies_target_registers);
    RUN_TEST(test_context_from_exception_frame_rejects_null_arguments);
    RUN_TEST(test_fatal_config_rejects_invalid_output_and_bounds);
    RUN_TEST(test_fatal_process_captures_backtrace_and_dumps_output);
    RUN_TEST(test_fatal_process_rejects_null_context);
    RUN_TEST(test_cp15_capture_returns_unsupported_on_host);
    RUN_TEST(test_cp15_capture_rejects_null_pointer);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
