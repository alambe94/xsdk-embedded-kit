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

// @file test_xcli.c
// @brief Host tests for xCLI text line parsing and dispatch adapter.

#include <string.h>
#include <stdint.h>

#include "unity.h"

#include "xcli.h"
#include "xcmd.h"
#include "xshell_config.h"
#include "xshell_return.h"

// Override the weak halt symbol so null-pointer tests continue past assertions.
void xassert_system_halt(void)
{
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

#define ARGV_CAP 8U

static char g_line[128U];
static char *g_argv[ARGV_CAP];
static size_t g_argc;

// Copy a string literal into the mutable g_line buffer before each parse call.
static void set_line(const char *src)
{
    (void)strncpy(g_line, src, sizeof(g_line) - 1U);
    g_line[sizeof(g_line) - 1U] = '\0';
}

static xCMD_Context_t g_cmd_ctx;
static xCMD_Command_t g_cmd_table[8U];
static uint32_t g_dispatch_count;
static xCMD_Request_t g_last_request;

static xRETURN_t capture_callback(xCMD_Request_t *request)
{
    g_last_request = *request;
    g_dispatch_count++;
    return xRETURN_OK;
}

void setUp(void)
{
    g_argc = 0U;
    g_dispatch_count = 0U;
    (void)memset(g_line, 0, sizeof(g_line));
    (void)xCMD_Init(&g_cmd_ctx, g_cmd_table, 8U);
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// xCLI_Parse_Line - null argument guards
// ---------------------------------------------------------------------------

void test_parse_null_line_returns_null_pointer_error(void)
{
    xRETURN_t ret = xCLI_Parse_Line(NULL, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_parse_null_argv_returns_null_pointer_error(void)
{
    set_line("cmd");
    xRETURN_t ret = xCLI_Parse_Line(g_line, NULL, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_parse_null_argc_returns_null_pointer_error(void)
{
    set_line("cmd");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_parse_zero_capacity_returns_invalid_arg(void)
{
    set_line("cmd");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, 0U, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

// ---------------------------------------------------------------------------
// xCLI_Parse_Line - empty and whitespace input
// ---------------------------------------------------------------------------

void test_parse_empty_string_returns_no_command(void)
{
    set_line("");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xMSG_xSHELL_NO_COMMAND, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_argc);
}

void test_parse_whitespace_only_returns_no_command(void)
{
    set_line("   \t  ");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xMSG_xSHELL_NO_COMMAND, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_argc);
}

// ---------------------------------------------------------------------------
// xCLI_Parse_Line - token counting and values
// ---------------------------------------------------------------------------

void test_parse_single_token(void)
{
    set_line("hello");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, g_argc);
    TEST_ASSERT_EQUAL_STRING("hello", g_argv[0]);
}

void test_parse_multiple_tokens(void)
{
    set_line("cmd arg1 arg2 arg3");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(4U, g_argc);
    TEST_ASSERT_EQUAL_STRING("cmd", g_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg1", g_argv[1]);
    TEST_ASSERT_EQUAL_STRING("arg2", g_argv[2]);
    TEST_ASSERT_EQUAL_STRING("arg3", g_argv[3]);
}

void test_parse_leading_and_trailing_whitespace(void)
{
    set_line("  cmd  arg1  ");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2U, g_argc);
    TEST_ASSERT_EQUAL_STRING("cmd", g_argv[0]);
    TEST_ASSERT_EQUAL_STRING("arg1", g_argv[1]);
}

// ---------------------------------------------------------------------------
// xCLI_Parse_Line - quoted tokens
// ---------------------------------------------------------------------------

void test_parse_double_quoted_token(void)
{
    set_line("cmd \"hello world\"");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2U, g_argc);
    TEST_ASSERT_EQUAL_STRING("cmd", g_argv[0]);
    TEST_ASSERT_EQUAL_STRING("hello world", g_argv[1]);
}

void test_parse_single_quoted_token(void)
{
    set_line("cmd 'hello world'");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2U, g_argc);
    TEST_ASSERT_EQUAL_STRING("hello world", g_argv[1]);
}

void test_parse_mixed_quoted_and_unquoted(void)
{
    set_line("cmd plain \"spaced arg\"");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(3U, g_argc);
    TEST_ASSERT_EQUAL_STRING("plain", g_argv[1]);
    TEST_ASSERT_EQUAL_STRING("spaced arg", g_argv[2]);
}

void test_parse_empty_double_quoted_token(void)
{
    set_line("cmd \"\"");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2U, g_argc);
    TEST_ASSERT_EQUAL_STRING("", g_argv[1]);
}

// ---------------------------------------------------------------------------
// xCLI_Parse_Line - error cases
// ---------------------------------------------------------------------------

void test_parse_unterminated_double_quote_returns_invalid_arg(void)
{
    set_line("cmd \"unterminated");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_argc);
}

void test_parse_unterminated_single_quote_returns_invalid_arg(void)
{
    set_line("cmd 'unterminated");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, ARGV_CAP, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_argc);
}

void test_parse_too_many_args_returns_buffer_full(void)
{
    // 3 tokens with argv_capacity of 2 should overflow on the third token.
    set_line("a b c");
    xRETURN_t ret = xCLI_Parse_Line(g_line, g_argv, 2U, &g_argc);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_BUFFER_FULL, ret);
}

// ---------------------------------------------------------------------------
// xCLI_Execute_Line
// ---------------------------------------------------------------------------

void test_execute_null_cmd_ctx_returns_null_pointer_error(void)
{
    set_line("cmd");
    xRETURN_t ret = xCLI_Execute_Line(NULL, g_line, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_execute_null_line_returns_null_pointer_error(void)
{
    xRETURN_t ret = xCLI_Execute_Line(&g_cmd_ctx, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_execute_empty_line_returns_no_command(void)
{
    set_line("");
    xRETURN_t ret = xCLI_Execute_Line(&g_cmd_ctx, g_line, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xMSG_xSHELL_NO_COMMAND, ret);
}

void test_execute_dispatches_registered_command(void)
{
    static const xCMD_Command_t cmd = {.path = "run", .callback = capture_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    set_line("run");
    xRETURN_t ret = xCLI_Execute_Line(&g_cmd_ctx, g_line, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, g_dispatch_count);
}

void test_execute_forwards_args_to_callback(void)
{
    static const xCMD_Command_t cmd = {.path = "greet", .callback = capture_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    set_line("greet Alice Bob");
    (void)xCLI_Execute_Line(&g_cmd_ctx, g_line, NULL);

    TEST_ASSERT_EQUAL_UINT32(3U, g_last_request.argc);
    TEST_ASSERT_EQUAL_STRING("greet", g_last_request.argv[0]);
    TEST_ASSERT_EQUAL_STRING("Alice", g_last_request.argv[1]);
    TEST_ASSERT_EQUAL_STRING("Bob", g_last_request.argv[2]);
}

void test_execute_sets_source_to_cli(void)
{
    static const xCMD_Command_t cmd = {.path = "src_test", .callback = capture_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    set_line("src_test");
    (void)xCLI_Execute_Line(&g_cmd_ctx, g_line, NULL);

    TEST_ASSERT_EQUAL_UINT32(xCMD_SOURCE_CLI, (uint32_t)g_last_request.source);
}

void test_execute_forwards_session_ctx(void)
{
    static const xCMD_Command_t cmd = {.path = "sess_test", .callback = capture_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    static uint32_t sentinel = 0xDEADBEEFU;
    set_line("sess_test");
    (void)xCLI_Execute_Line(&g_cmd_ctx, g_line, &sentinel);

    TEST_ASSERT_EQUAL_PTR(&sentinel, g_last_request.session_ctx);
}

void test_execute_unknown_command_returns_not_found(void)
{
    set_line("no_such_cmd");
    xRETURN_t ret = xCLI_Execute_Line(&g_cmd_ctx, g_line, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NOT_FOUND, ret);
}

// ---------------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_null_line_returns_null_pointer_error);
    RUN_TEST(test_parse_null_argv_returns_null_pointer_error);
    RUN_TEST(test_parse_null_argc_returns_null_pointer_error);
    RUN_TEST(test_parse_zero_capacity_returns_invalid_arg);
    RUN_TEST(test_parse_empty_string_returns_no_command);
    RUN_TEST(test_parse_whitespace_only_returns_no_command);
    RUN_TEST(test_parse_single_token);
    RUN_TEST(test_parse_multiple_tokens);
    RUN_TEST(test_parse_leading_and_trailing_whitespace);
    RUN_TEST(test_parse_double_quoted_token);
    RUN_TEST(test_parse_single_quoted_token);
    RUN_TEST(test_parse_mixed_quoted_and_unquoted);
    RUN_TEST(test_parse_empty_double_quoted_token);
    RUN_TEST(test_parse_unterminated_double_quote_returns_invalid_arg);
    RUN_TEST(test_parse_unterminated_single_quote_returns_invalid_arg);
    RUN_TEST(test_parse_too_many_args_returns_buffer_full);
    RUN_TEST(test_execute_null_cmd_ctx_returns_null_pointer_error);
    RUN_TEST(test_execute_null_line_returns_null_pointer_error);
    RUN_TEST(test_execute_empty_line_returns_no_command);
    RUN_TEST(test_execute_dispatches_registered_command);
    RUN_TEST(test_execute_forwards_args_to_callback);
    RUN_TEST(test_execute_sets_source_to_cli);
    RUN_TEST(test_execute_forwards_session_ctx);
    RUN_TEST(test_execute_unknown_command_returns_not_found);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
