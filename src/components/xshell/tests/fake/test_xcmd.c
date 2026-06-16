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

// @file test_xcmd.c
// @brief Host tests for xCMD command table registration and dispatch.

#include <stdint.h>

#include "unity.h"

#include "xcmd.h"
#include "xshell_return.h"

// xassert_system_halt() has __attribute__((weak)) in xassert.h.
// This strong definition overrides it so that tests which deliberately pass
// invalid arguments can continue past the assertion and reach the error return.
void xassert_system_halt(void)
{
}

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

#define TABLE_SIZE 4U

static xCMD_Context_t g_cmd_ctx;
static xCMD_Command_t g_table[TABLE_SIZE];
static uint32_t g_callback_calls;
static xRETURN_t g_callback_ret;
static xCMD_Request_t g_last_request;

static xRETURN_t stub_callback(xCMD_Request_t *request)
{
    g_last_request = *request;
    g_callback_calls++;
    return g_callback_ret;
}

static xRETURN_t stub_callback_b(xCMD_Request_t *request)
{
    (void)request;
    return xRETURN_OK;
}

void setUp(void)
{
    g_callback_calls = 0U;
    g_callback_ret = xRETURN_OK;
    (void)xCMD_Init(&g_cmd_ctx, g_table, TABLE_SIZE);
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// xCMD_Init tests
// ---------------------------------------------------------------------------

void test_init_null_ctx_returns_null_pointer_error(void)
{
    xRETURN_t ret = xCMD_Init(NULL, g_table, TABLE_SIZE);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_init_null_table_returns_null_pointer_error(void)
{
    xCMD_Context_t ctx;
    xRETURN_t ret = xCMD_Init(&ctx, NULL, TABLE_SIZE);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_init_zero_capacity_returns_invalid_arg(void)
{
    xCMD_Context_t ctx;
    xRETURN_t ret = xCMD_Init(&ctx, g_table, 0U);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_init_success_sets_count_to_zero_and_capacity(void)
{
    xCMD_Context_t ctx;
    xRETURN_t ret = xCMD_Init(&ctx, g_table, TABLE_SIZE);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, ctx.count);
    TEST_ASSERT_EQUAL_UINT32(TABLE_SIZE, ctx.capacity);
}

// ---------------------------------------------------------------------------
// xCMD_Register tests
// ---------------------------------------------------------------------------

void test_register_null_ctx_returns_null_pointer_error(void)
{
    static const xCMD_Command_t cmd = {.path = "foo", .callback = stub_callback};
    xRETURN_t ret = xCMD_Register(NULL, &cmd);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_register_null_command_returns_null_pointer_error(void)
{
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_register_null_path_returns_invalid_arg(void)
{
    static const xCMD_Command_t cmd = {.path = NULL, .callback = stub_callback};
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, &cmd);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_register_null_callback_returns_invalid_arg(void)
{
    static const xCMD_Command_t cmd = {.path = "foo", .callback = NULL};
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, &cmd);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_register_success_increments_count(void)
{
    static const xCMD_Command_t cmd = {.path = "ping", .callback = stub_callback};
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, &cmd);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_ctx.count);
}

void test_register_duplicate_path_returns_invalid_arg(void)
{
    static const xCMD_Command_t cmd = {.path = "dup", .callback = stub_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, &cmd);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_ctx.count);
}

void test_register_full_registry_returns_buffer_full(void)
{
    static const xCMD_Command_t cmds[TABLE_SIZE + 1U] = {
        {.path = "a", .callback = stub_callback}, {.path = "b", .callback = stub_callback}, {.path = "c", .callback = stub_callback},
        {.path = "d", .callback = stub_callback}, {.path = "e", .callback = stub_callback},
    };
    for (uint32_t i = 0U; i < TABLE_SIZE; i++)
    {
        (void)xCMD_Register(&g_cmd_ctx, &cmds[i]);
    }
    xRETURN_t ret = xCMD_Register(&g_cmd_ctx, &cmds[TABLE_SIZE]);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_BUFFER_FULL, ret);
    TEST_ASSERT_EQUAL_UINT32(TABLE_SIZE, g_cmd_ctx.count);
}

// ---------------------------------------------------------------------------
// xCMD_Dispatch tests
// ---------------------------------------------------------------------------

void test_dispatch_null_ctx_returns_null_pointer_error(void)
{
    const char *argv[] = {"foo"};
    xCMD_Request_t request = {.path = "foo", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    xRETURN_t ret = xCMD_Dispatch(NULL, &request);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_dispatch_null_request_returns_null_pointer_error(void)
{
    xRETURN_t ret = xCMD_Dispatch(&g_cmd_ctx, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_dispatch_null_path_returns_invalid_arg(void)
{
    xCMD_Request_t request = {.path = NULL, .argc = 0U, .argv = NULL, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    xRETURN_t ret = xCMD_Dispatch(&g_cmd_ctx, &request);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_dispatch_unknown_command_returns_not_found(void)
{
    const char *argv[] = {"unknown"};
    xCMD_Request_t request = {
        .path = "unknown", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    xRETURN_t ret = xCMD_Dispatch(&g_cmd_ctx, &request);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NOT_FOUND, ret);
}

void test_dispatch_calls_matching_callback(void)
{
    static const xCMD_Command_t cmd = {.path = "go", .callback = stub_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    const char *argv[] = {"go"};
    xCMD_Request_t request = {.path = "go", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    xRETURN_t ret = xCMD_Dispatch(&g_cmd_ctx, &request);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, g_callback_calls);
}

void test_dispatch_does_not_call_wrong_callback(void)
{
    static const xCMD_Command_t cmd_a = {.path = "aaa", .callback = stub_callback};
    static const xCMD_Command_t cmd_b = {.path = "bbb", .callback = stub_callback_b};
    (void)xCMD_Register(&g_cmd_ctx, &cmd_a);
    (void)xCMD_Register(&g_cmd_ctx, &cmd_b);

    const char *argv[] = {"bbb"};
    xCMD_Request_t request = {.path = "bbb", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    (void)xCMD_Dispatch(&g_cmd_ctx, &request);

    // stub_callback (registered for "aaa") must not have been called.
    TEST_ASSERT_EQUAL_UINT32(0U, g_callback_calls);
}

void test_dispatch_propagates_callback_return_value(void)
{
    g_callback_ret = xRETURN_xERR_xSHELL_INVALID_STATE;

    static const xCMD_Command_t cmd = {.path = "fail", .callback = stub_callback};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    const char *argv[] = {"fail"};
    xCMD_Request_t request = {
        .path = "fail", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_DIRECT, .session_ctx = NULL, .user_ctx = NULL};
    xRETURN_t ret = xCMD_Dispatch(&g_cmd_ctx, &request);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

void test_dispatch_sets_user_ctx_from_command_ctx(void)
{
    static uint32_t my_state = 42U;
    static const xCMD_Command_t cmd = {
        .path = "ctx_test",
        .callback = stub_callback,
        .command_ctx = &my_state,
    };
    (void)xCMD_Register(&g_cmd_ctx, &cmd);

    const char *argv[] = {"ctx_test"};
    xCMD_Request_t request = {
        .path = "ctx_test", .argc = 1U, .argv = argv, .source = xCMD_SOURCE_TEST, .session_ctx = NULL, .user_ctx = NULL};
    (void)xCMD_Dispatch(&g_cmd_ctx, &request);

    TEST_ASSERT_EQUAL_PTR(&my_state, g_last_request.user_ctx);
}

// ---------------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_init_null_table_returns_null_pointer_error);
    RUN_TEST(test_init_zero_capacity_returns_invalid_arg);
    RUN_TEST(test_init_success_sets_count_to_zero_and_capacity);
    RUN_TEST(test_register_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_register_null_command_returns_null_pointer_error);
    RUN_TEST(test_register_null_path_returns_invalid_arg);
    RUN_TEST(test_register_null_callback_returns_invalid_arg);
    RUN_TEST(test_register_success_increments_count);
    RUN_TEST(test_register_duplicate_path_returns_invalid_arg);
    RUN_TEST(test_register_full_registry_returns_buffer_full);
    RUN_TEST(test_dispatch_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_dispatch_null_request_returns_null_pointer_error);
    RUN_TEST(test_dispatch_null_path_returns_invalid_arg);
    RUN_TEST(test_dispatch_unknown_command_returns_not_found);
    RUN_TEST(test_dispatch_calls_matching_callback);
    RUN_TEST(test_dispatch_does_not_call_wrong_callback);
    RUN_TEST(test_dispatch_propagates_callback_return_value);
    RUN_TEST(test_dispatch_sets_user_ctx_from_command_ctx);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
