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

// @file test_xbridge_gpio.c
// @brief Host unit tests for the xBRIDGE GPIO bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_gpio.h"
#include "xbridge_core.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_GPIO_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void gpio_init_succeeds_with_valid_ops(void);
void gpio_init_rejects_null_ctx(void);
void gpio_init_rejects_null_usb_ops(void);
void gpio_init_rejects_null_gpio_ops(void);
void gpio_set_direction_calls_peripheral(void);
void gpio_write_pin_calls_peripheral(void);
void gpio_read_pin_returns_value_in_response(void);
void gpio_set_pull_calls_peripheral(void);
void gpio_toggle_pin_calls_peripheral(void);
void gpio_unknown_cmd_returns_error_response(void);

// Helpers /////////////////////////////////////////////////////////////////////////

static void build_frame_port_pin_u8(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint32_t port, uint32_t pin, uint8_t val)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_GPIO;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 9U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(port & 0xFFU);
    buf[off++] = (uint8_t)((port >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((port >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((port >> 24U) & 0xFFU);
    buf[off++] = (uint8_t)(pin & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 24U) & 0xFFU);
    buf[off++] = val;
    *len = off;
}

static void build_frame_port_pin(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint32_t port, uint32_t pin)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_GPIO;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 8U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(port & 0xFFU);
    buf[off++] = (uint8_t)((port >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((port >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((port >> 24U) & 0xFFU);
    buf[off++] = (uint8_t)(pin & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((pin >> 24U) & 0xFFU);
    *len = off;
}

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_GPIO_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_gpio_ops, NULL);
}

void tearDown(void)
{
}

void gpio_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_GPIO_Context_t ctx;
    xRETURN_t ret = xBRIDGE_GPIO_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_gpio_ops, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_PTR(&g_mock_gpio_ops, ctx.gpio_ops);
}

void gpio_init_rejects_null_ctx(void)
{
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xBRIDGE_GPIO_Init(NULL, &g_mock_usb_ops, NULL, &g_mock_gpio_ops, NULL));
}

void gpio_init_rejects_null_usb_ops(void)
{
    xBRIDGE_GPIO_Context_t ctx;
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xBRIDGE_GPIO_Init(&ctx, NULL, NULL, &g_mock_gpio_ops, NULL));
}

void gpio_init_rejects_null_gpio_ops(void)
{
    xBRIDGE_GPIO_Context_t ctx;
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xBRIDGE_GPIO_Init(&ctx, &g_mock_usb_ops, NULL, NULL, NULL));
}

void gpio_set_direction_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_port_pin_u8(buf, &len, xBRIDGE_GPIO_CMD_SET_DIRECTION, 1U, 0U, 5U, xBRIDGE_GPIO_DIR_OUTPUT);

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpio_set_direction_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpio_set_direction_fake.arg1_val); // port
    TEST_ASSERT_EQUAL_UINT32(5U, mock_gpio_set_direction_fake.arg2_val); // pin
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_GPIO_DIR_OUTPUT, mock_gpio_set_direction_fake.arg3_val);
}

void gpio_write_pin_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_port_pin_u8(buf, &len, xBRIDGE_GPIO_CMD_WRITE, 2U, 0U, 3U, 1U);

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpio_write_pin_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(3U, mock_gpio_write_pin_fake.arg2_val); // pin
    TEST_ASSERT_EQUAL_UINT8(1U, mock_gpio_write_pin_fake.arg3_val);  // value
}

void gpio_read_pin_returns_value_in_response(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_port_pin(buf, &len, xBRIDGE_GPIO_CMD_READ, 3U, 0U, 7U);

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpio_read_pin_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void gpio_set_pull_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_port_pin_u8(buf, &len, xBRIDGE_GPIO_CMD_SET_PULL, 4U, 0U, 2U, xBRIDGE_GPIO_PULL_UP);

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpio_set_pull_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_GPIO_PULL_UP, mock_gpio_set_pull_fake.arg3_val);
}

void gpio_toggle_pin_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_port_pin(buf, &len, xBRIDGE_GPIO_CMD_TOGGLE, 5U, 0U, 4U);

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpio_toggle_pin_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(4U, mock_gpio_toggle_pin_fake.arg2_val);
}

void gpio_unknown_cmd_returns_error_response(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_GPIO;
    hdr.cmd = 0xFFU;
    hdr.seq = 6U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_GPIO_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(gpio_init_succeeds_with_valid_ops);
    RUN_TEST(gpio_init_rejects_null_ctx);
    RUN_TEST(gpio_init_rejects_null_usb_ops);
    RUN_TEST(gpio_init_rejects_null_gpio_ops);
    RUN_TEST(gpio_set_direction_calls_peripheral);
    RUN_TEST(gpio_write_pin_calls_peripheral);
    RUN_TEST(gpio_read_pin_returns_value_in_response);
    RUN_TEST(gpio_set_pull_calls_peripheral);
    RUN_TEST(gpio_toggle_pin_calls_peripheral);
    RUN_TEST(gpio_unknown_cmd_returns_error_response);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
