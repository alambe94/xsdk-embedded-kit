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

// @file test_xshell.c
// @brief Host tests for xSHELL session lifecycle and fake transport processing.

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "unity.h"

#include "xshell.h"
#include "xcmd.h"
#include "xshell_return.h"

// Override the weak halt symbol so null-pointer tests continue past assertions.
void xassert_system_halt(void)
{
}

// ---------------------------------------------------------------------------
// Fake transport
// ---------------------------------------------------------------------------

#define FAKE_RX_BUF_SIZE 256U
#define FAKE_TX_BUF_SIZE 512U

static uint8_t g_rx_buf[FAKE_RX_BUF_SIZE];
static size_t g_rx_len;
static size_t g_rx_pos;

static uint8_t g_tx_buf[FAKE_TX_BUF_SIZE];
static size_t g_tx_len;

static xRETURN_t fake_read(void *ctx, uint8_t *buf, size_t buf_len, size_t *bytes_read)
{
    (void)ctx;
    size_t available = g_rx_len - g_rx_pos;
    size_t to_copy = (available < buf_len) ? available : buf_len;
    for (size_t i = 0U; i < to_copy; i++)
    {
        buf[i] = g_rx_buf[g_rx_pos + i];
    }
    g_rx_pos += to_copy;
    *bytes_read = to_copy;
    return xRETURN_OK;
}

static xRETURN_t fake_write(void *ctx, const uint8_t *buf, size_t len, size_t *bytes_written)
{
    (void)ctx;
    for (size_t i = 0U; i < len; i++)
    {
        if (g_tx_len < FAKE_TX_BUF_SIZE)
        {
            g_tx_buf[g_tx_len] = buf[i];
            g_tx_len++;
        }
    }
    *bytes_written = len;
    return xRETURN_OK;
}

static const xSHELL_Transport_t g_transport = {
    .read = fake_read,
    .write = fake_write,
    .flush = NULL,
};

// Feed bytes into the fake RX buffer.
static void feed(const char *str)
{
    size_t len = strlen(str);
    for (size_t i = 0U; i < len; i++)
    {
        if (g_rx_len < FAKE_RX_BUF_SIZE)
        {
            g_rx_buf[g_rx_len] = (uint8_t)str[i];
            g_rx_len++;
        }
    }
}

static void feed_bytes(const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; i++)
    {
        if (g_rx_len < FAKE_RX_BUF_SIZE)
        {
            g_rx_buf[g_rx_len] = data[i];
            g_rx_len++;
        }
    }
}

// ---------------------------------------------------------------------------
// Command fixture
// ---------------------------------------------------------------------------

#define CMD_TABLE_SIZE 8U

static xCMD_Context_t g_cmd_ctx;
static xCMD_Command_t g_cmd_table[CMD_TABLE_SIZE];
static uint32_t g_cmd_dispatch_count;

static xRETURN_t stub_cmd(xCMD_Request_t *request)
{
    (void)request;
    g_cmd_dispatch_count++;
    return xRETURN_OK;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

static xSHELL_Context_t g_shell;

static void reset_transport(void)
{
    (void)memset(g_rx_buf, 0, sizeof(g_rx_buf));
    (void)memset(g_tx_buf, 0, sizeof(g_tx_buf));
    g_rx_len = 0U;
    g_rx_pos = 0U;
    g_tx_len = 0U;
}

void setUp(void)
{
    reset_transport();
    g_cmd_dispatch_count = 0U;
    (void)xCMD_Init(&g_cmd_ctx, g_cmd_table, CMD_TABLE_SIZE);
}

void tearDown(void)
{
}

// ---------------------------------------------------------------------------
// Helper: fully initialised and started shell
// ---------------------------------------------------------------------------

static xSHELL_Config_t make_config(const char *prompt)
{
    xSHELL_Config_t cfg;
    cfg.cmd_ctx = &g_cmd_ctx;
    cfg.transport = &g_transport;
    cfg.transport_ctx = NULL;
    cfg.prompt = prompt;
    return cfg;
}

static void init_and_start(const char *prompt)
{
    xSHELL_Config_t cfg = make_config(prompt);
    (void)xSHELL_Init(&g_shell, &cfg);
    reset_transport();
    (void)xSHELL_Start(&g_shell);
    reset_transport();
}

// ---------------------------------------------------------------------------
// xSHELL_Init tests
// ---------------------------------------------------------------------------

void test_init_null_ctx_returns_null_pointer_error(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    xRETURN_t ret = xSHELL_Init(NULL, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_init_null_config_returns_null_pointer_error(void)
{
    xRETURN_t ret = xSHELL_Init(&g_shell, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_init_null_cmd_ctx_returns_invalid_arg(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    cfg.cmd_ctx = NULL;
    xRETURN_t ret = xSHELL_Init(&g_shell, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_init_null_transport_returns_invalid_arg(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    cfg.transport = NULL;
    xRETURN_t ret = xSHELL_Init(&g_shell, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_ARG, ret);
}

void test_init_success_sets_state_ready(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    xRETURN_t ret = xSHELL_Init(&g_shell, &cfg);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(xSHELL_STATE_READY, (uint32_t)g_shell.state);
}

void test_init_copies_prompt_string(void)
{
    xSHELL_Config_t cfg = make_config("> ");
    (void)xSHELL_Init(&g_shell, &cfg);
    TEST_ASSERT_EQUAL_STRING("> ", g_shell.prompt);
}

void test_init_null_prompt_sets_empty_prompt(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    (void)xSHELL_Init(&g_shell, &cfg);
    TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)g_shell.prompt[0]);
}

// ---------------------------------------------------------------------------
// xSHELL_Start tests
// ---------------------------------------------------------------------------

void test_start_null_ctx_returns_null_pointer_error(void)
{
    xRETURN_t ret = xSHELL_Start(NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_start_before_init_returns_invalid_state(void)
{
    (void)memset(&g_shell, 0, sizeof(g_shell));
    xRETURN_t ret = xSHELL_Start(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

void test_start_transitions_to_running(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    (void)xSHELL_Init(&g_shell, &cfg);
    xRETURN_t ret = xSHELL_Start(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(xSHELL_STATE_RUNNING, (uint32_t)g_shell.state);
}

void test_start_emits_prompt(void)
{
    xSHELL_Config_t cfg = make_config("$ ");
    (void)xSHELL_Init(&g_shell, &cfg);
    reset_transport();
    (void)xSHELL_Start(&g_shell);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, g_tx_len);
    g_tx_buf[g_tx_len] = '\0';
    TEST_ASSERT_EQUAL_STRING("$ ", (const char *)g_tx_buf);
}

void test_start_twice_returns_invalid_state(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    (void)xSHELL_Init(&g_shell, &cfg);
    (void)xSHELL_Start(&g_shell);
    xRETURN_t ret = xSHELL_Start(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

// ---------------------------------------------------------------------------
// xSHELL_Stop tests
// ---------------------------------------------------------------------------

void test_stop_null_ctx_returns_null_pointer_error(void)
{
    xRETURN_t ret = xSHELL_Stop(NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_stop_before_start_returns_invalid_state(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    (void)xSHELL_Init(&g_shell, &cfg);
    xRETURN_t ret = xSHELL_Stop(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

void test_stop_transitions_to_stopped(void)
{
    init_and_start(NULL);
    xRETURN_t ret = xSHELL_Stop(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(xSHELL_STATE_STOPPED, (uint32_t)g_shell.state);
}

// ---------------------------------------------------------------------------
// xSHELL_Process tests
// ---------------------------------------------------------------------------

void test_process_null_ctx_returns_null_pointer_error(void)
{
    xRETURN_t ret = xSHELL_Process(NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_process_before_start_returns_invalid_state(void)
{
    xSHELL_Config_t cfg = make_config(NULL);
    (void)xSHELL_Init(&g_shell, &cfg);
    xRETURN_t ret = xSHELL_Process(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

void test_process_complete_line_with_lf_dispatches_command(void)
{
    static const xCMD_Command_t cmd = {.path = "ping", .callback = stub_cmd};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    init_and_start(NULL);

    feed("ping\n");
    (void)xSHELL_Process(&g_shell);

    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_dispatch_count);
}

void test_process_complete_line_with_cr_dispatches_command(void)
{
    static const xCMD_Command_t cmd = {.path = "pong", .callback = stub_cmd};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    init_and_start(NULL);

    feed("pong\r");
    (void)xSHELL_Process(&g_shell);

    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_dispatch_count);
}

void test_process_crlf_dispatches_once(void)
{
    static const xCMD_Command_t cmd = {.path = "crlf", .callback = stub_cmd};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    init_and_start(NULL);

    feed("crlf\r\n");
    (void)xSHELL_Process(&g_shell);
    (void)xSHELL_Process(&g_shell);

    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_dispatch_count);
}

void test_process_partial_lines_accumulate(void)
{
    static const xCMD_Command_t cmd = {.path = "partial", .callback = stub_cmd};
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    init_and_start(NULL);

    feed("par");
    (void)xSHELL_Process(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(0U, g_cmd_dispatch_count);

    feed("tial\n");
    (void)xSHELL_Process(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(1U, g_cmd_dispatch_count);
}

void test_process_empty_line_does_not_dispatch(void)
{
    init_and_start(NULL);

    feed("\n");
    (void)xSHELL_Process(&g_shell);

    TEST_ASSERT_EQUAL_UINT32(0U, g_cmd_dispatch_count);
}

void test_process_overflow_line_discards_and_continues(void)
{
    init_and_start(NULL);

    // Fill beyond the line buffer limit.
    for (uint32_t i = 0U; i < (xSHELL_MAX_LINE_LENGTH + 10U); i++)
    {
        uint8_t byte = (uint8_t)'x';
        feed_bytes(&byte, 1U);
    }
    feed("\n");
    (void)xSHELL_Process(&g_shell);
    (void)xSHELL_Process(&g_shell);

    // Overflow line must not dispatch; shell should still be running.
    TEST_ASSERT_EQUAL_UINT32(0U, g_cmd_dispatch_count);
    TEST_ASSERT_EQUAL_UINT32(xSHELL_STATE_RUNNING, (uint32_t)g_shell.state);
}

void test_process_after_stop_returns_invalid_state(void)
{
    init_and_start(NULL);
    (void)xSHELL_Stop(&g_shell);
    xRETURN_t ret = xSHELL_Process(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_INVALID_STATE, ret);
}

// ---------------------------------------------------------------------------
// xSHELL_Write / xSHELL_Write_String tests
// ---------------------------------------------------------------------------

void test_write_null_ctx_returns_null_pointer_error(void)
{
    static const uint8_t data[] = {0x41U};
    xRETURN_t ret = xSHELL_Write(NULL, data, 1U);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_write_null_data_returns_null_pointer_error(void)
{
    init_and_start(NULL);
    xRETURN_t ret = xSHELL_Write(&g_shell, NULL, 4U);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xSHELL_NULL_POINTER, ret);
}

void test_write_string_sends_bytes(void)
{
    init_and_start(NULL);
    reset_transport();
    (void)xSHELL_Write_String(&g_shell, "hi");
    TEST_ASSERT_EQUAL_UINT32(2U, g_tx_len);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'h', g_tx_buf[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)'i', g_tx_buf[1]);
}

void test_write_string_empty_is_noop(void)
{
    init_and_start(NULL);
    reset_transport();
    (void)xSHELL_Write_String(&g_shell, "");
    TEST_ASSERT_EQUAL_UINT32(0U, g_tx_len);
}

// ---------------------------------------------------------------------------
// Built-in commands
// ---------------------------------------------------------------------------

void test_builtins_register_help_and_echo(void)
{
    init_and_start(NULL);
    xRETURN_t ret = xSHELL_Register_Builtins(&g_shell);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2U, g_cmd_ctx.count);
}

void test_builtin_echo_writes_args(void)
{
    init_and_start(NULL);
    (void)xSHELL_Register_Builtins(&g_shell);

    reset_transport();
    // "echo hi\n" is 8 bytes - fits in one READ_CHUNK_SIZE (16) read.
    feed("echo hi\n");
    (void)xSHELL_Process(&g_shell);

    g_tx_buf[g_tx_len] = '\0';
    TEST_ASSERT_NOT_NULL((const void *)strstr((const char *)g_tx_buf, "hi"));
}

void test_builtin_help_lists_commands(void)
{
    static const xCMD_Command_t cmd = {
        .path = "status",
        .summary = "Show status",
        .callback = stub_cmd,
    };
    (void)xCMD_Register(&g_cmd_ctx, &cmd);
    init_and_start(NULL);
    (void)xSHELL_Register_Builtins(&g_shell);

    reset_transport();
    feed("help\n");
    (void)xSHELL_Process(&g_shell);

    g_tx_buf[g_tx_len] = '\0';
    TEST_ASSERT_NOT_NULL((const void *)strstr((const char *)g_tx_buf, "status"));
}

// ---------------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_init_null_config_returns_null_pointer_error);
    RUN_TEST(test_init_null_cmd_ctx_returns_invalid_arg);
    RUN_TEST(test_init_null_transport_returns_invalid_arg);
    RUN_TEST(test_init_success_sets_state_ready);
    RUN_TEST(test_init_copies_prompt_string);
    RUN_TEST(test_init_null_prompt_sets_empty_prompt);
    RUN_TEST(test_start_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_start_before_init_returns_invalid_state);
    RUN_TEST(test_start_transitions_to_running);
    RUN_TEST(test_start_emits_prompt);
    RUN_TEST(test_start_twice_returns_invalid_state);
    RUN_TEST(test_stop_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_stop_before_start_returns_invalid_state);
    RUN_TEST(test_stop_transitions_to_stopped);
    RUN_TEST(test_process_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_process_before_start_returns_invalid_state);
    RUN_TEST(test_process_complete_line_with_lf_dispatches_command);
    RUN_TEST(test_process_complete_line_with_cr_dispatches_command);
    RUN_TEST(test_process_crlf_dispatches_once);
    RUN_TEST(test_process_partial_lines_accumulate);
    RUN_TEST(test_process_empty_line_does_not_dispatch);
    RUN_TEST(test_process_overflow_line_discards_and_continues);
    RUN_TEST(test_process_after_stop_returns_invalid_state);
    RUN_TEST(test_write_null_ctx_returns_null_pointer_error);
    RUN_TEST(test_write_null_data_returns_null_pointer_error);
    RUN_TEST(test_write_string_sends_bytes);
    RUN_TEST(test_write_string_empty_is_noop);
    RUN_TEST(test_builtins_register_help_and_echo);
    RUN_TEST(test_builtin_echo_writes_args);
    RUN_TEST(test_builtin_help_lists_commands);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
