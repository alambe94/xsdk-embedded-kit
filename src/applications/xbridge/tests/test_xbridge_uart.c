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

// @file test_xbridge_uart.c
// @brief Host unit tests for the xBRIDGE UART bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_uart.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_UART_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void uart_init_succeeds_with_valid_ops(void);
void uart_init_rejects_null_ctx(void);
void uart_init_rejects_null_usb_ops(void);
void uart_init_rejects_null_uart_ops(void);
void uart_on_line_coding_calls_peripheral_set_line_coding(void);
void uart_on_usb_receive_enqueues_bytes(void);
void uart_on_usb_receive_discards_when_dtr_inactive(void);
void uart_poll_drains_uart_rx_to_usb_send(void);
void uart_dtr_false_does_not_forward_bytes(void);
void uart_queue_full_returns_error(void);

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
}

void tearDown(void)
{
}

void uart_init_succeeds_with_valid_ops(void)
{
    xRETURN_t ret = xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_PTR(&g_mock_usb_ops, g_ctx.usb_ops);
    TEST_ASSERT_EQUAL_PTR(&g_mock_uart_ops, g_ctx.uart_ops);
}

void uart_init_rejects_null_ctx(void)
{
    xRETURN_t ret = xBRIDGE_UART_Init(NULL, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

void uart_init_rejects_null_usb_ops(void)
{
    xRETURN_t ret = xBRIDGE_UART_Init(&g_ctx, NULL, NULL, &g_mock_uart_ops, NULL);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

void uart_init_rejects_null_uart_ops(void)
{
    xRETURN_t ret = xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

void uart_on_line_coding_calls_peripheral_set_line_coding(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);

    xRETURN_t ret = xBRIDGE_UART_On_Line_Coding(&g_ctx, 115200U, 0U, 0U, 8U);

    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_uart_set_line_coding_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(115200U, mock_uart_set_line_coding_fake.arg1_val);
}

void uart_on_usb_receive_enqueues_bytes(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);

    xRETURN_t ret = xBRIDGE_UART_On_Control_Line_State(&g_ctx, true, false);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    const uint8_t data[4U] = {0x01U, 0x02U, 0x03U, 0x04U};
    ret = xBRIDGE_UART_On_USB_Receive(&g_ctx, data, sizeof(data));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    // Bytes should be enqueued; head stays 0, tail should advance
    TEST_ASSERT_EQUAL_UINT32(4U, g_ctx.usb_to_uart_tail);
}

void uart_on_usb_receive_discards_when_dtr_inactive(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);
    // DTR is not active (default after init)

    const uint8_t data[4U] = {0x01U, 0x02U, 0x03U, 0x04U};
    xRETURN_t ret = xBRIDGE_UART_On_USB_Receive(&g_ctx, data, sizeof(data));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_ctx.usb_to_uart_tail);
}

void uart_poll_drains_uart_rx_to_usb_send(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);

    mock_uart_is_rx_ready_fake.return_val = true;
    mock_uart_read_fake.return_val = xRETURN_OK;

    xRETURN_t ret = xBRIDGE_UART_Poll(&g_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    // uart_ops->read should have been called
    TEST_ASSERT_EQUAL_UINT32(1U, mock_uart_read_fake.call_count);
}

void uart_dtr_false_does_not_forward_bytes(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);
    (void)xBRIDGE_UART_On_Control_Line_State(&g_ctx, false, false);

    TEST_ASSERT_FALSE(g_ctx.is_dtr_active);

    const uint8_t data[2U] = {0xAAU, 0xBBU};
    xRETURN_t ret = xBRIDGE_UART_On_USB_Receive(&g_ctx, data, sizeof(data));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, g_ctx.usb_to_uart_tail);
}

void uart_queue_full_returns_error(void)
{
    (void)xBRIDGE_UART_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_uart_ops, NULL);
    (void)xBRIDGE_UART_On_Control_Line_State(&g_ctx, true, false);

    // Fill the queue
    uint8_t fill[xBRIDGE_UART_QUEUE_BYTES - 1U];
    (void)memset(fill, 0xFFU, sizeof(fill));
    (void)xBRIDGE_UART_On_USB_Receive(&g_ctx, fill, sizeof(fill));

    // One more byte should overflow
    const uint8_t extra[2U] = {0x01U, 0x02U};
    xRETURN_t ret = xBRIDGE_UART_On_USB_Receive(&g_ctx, extra, sizeof(extra));
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(uart_init_succeeds_with_valid_ops);
    RUN_TEST(uart_init_rejects_null_ctx);
    RUN_TEST(uart_init_rejects_null_usb_ops);
    RUN_TEST(uart_init_rejects_null_uart_ops);
    RUN_TEST(uart_on_line_coding_calls_peripheral_set_line_coding);
    RUN_TEST(uart_on_usb_receive_enqueues_bytes);
    RUN_TEST(uart_on_usb_receive_discards_when_dtr_inactive);
    RUN_TEST(uart_poll_drains_uart_rx_to_usb_send);
    RUN_TEST(uart_dtr_false_does_not_forward_bytes);
    RUN_TEST(uart_queue_full_returns_error);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
