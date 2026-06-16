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

// @file test_xbridge_dap.c
// @brief Host unit tests for the xBRIDGE CMSIS-DAP bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_dap.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_DAP_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void dap_init_succeeds_with_valid_ops(void);
void dap_info_returns_capabilities_byte(void);
void dap_connect_swd_sets_connect_mode(void);
void dap_disconnect_clears_connect_mode(void);
void dap_swj_clock_calls_peripheral_clock(void);
void dap_swj_sequence_calls_peripheral_sequence(void);
void dap_reset_target_calls_peripheral_reset(void);
void dap_delay_calls_peripheral_delay_us(void);
void dap_unknown_command_returns_ff_error(void);

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_DAP_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_dap_ops, NULL);
}

void tearDown(void)
{
}

void dap_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_DAP_Context_t ctx;
    xRETURN_t ret = xBRIDGE_DAP_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_dap_ops, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xBRIDGE_DAP_CONNECT_NONE, ctx.connect_mode);
    TEST_ASSERT_EQUAL_UINT32(1000000U, ctx.swj_clock_hz);
}

void dap_info_returns_capabilities_byte(void)
{
    const uint8_t req[1U] = {xBRIDGE_DAP_ID_INFO};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_DAP_ID_INFO, ((const uint8_t *)mock_usb_send_fake.arg1_val)[0]);
}

void dap_connect_swd_sets_connect_mode(void)
{
    const uint8_t req[2U] = {xBRIDGE_DAP_ID_CONNECT, (uint8_t)xBRIDGE_DAP_CONNECT_SWD};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xBRIDGE_DAP_CONNECT_SWD, g_ctx.connect_mode);
}

void dap_disconnect_clears_connect_mode(void)
{
    g_ctx.connect_mode = (uint8_t)xBRIDGE_DAP_CONNECT_SWD;

    const uint8_t req[1U] = {xBRIDGE_DAP_ID_DISCONNECT};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xBRIDGE_DAP_CONNECT_NONE, g_ctx.connect_mode);
}

void dap_swj_clock_calls_peripheral_clock(void)
{
    // DAP_SWJ_Clock with 1 MHz = 0x000F4240
    const uint8_t req[5U] = {xBRIDGE_DAP_ID_SWJ_CLOCK, 0x40U, 0x42U, 0x0FU, 0x00U};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_dap_swj_clock_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1000000U, mock_dap_swj_clock_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(1000000U, g_ctx.swj_clock_hz);
}

void dap_swj_sequence_calls_peripheral_sequence(void)
{
    // DAP_SWJ_Sequence: count=8 bits, 1 byte of data = 0xFF
    const uint8_t req[3U] = {xBRIDGE_DAP_ID_SWJ_SEQUENCE, 0x08U, 0xFFU};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_dap_swj_sequence_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(8U, mock_dap_swj_sequence_fake.arg1_val);
}

void dap_reset_target_calls_peripheral_reset(void)
{
    const uint8_t req[1U] = {xBRIDGE_DAP_ID_RESET_TARGET};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_dap_reset_target_fake.call_count);
}

void dap_delay_calls_peripheral_delay_us(void)
{
    // DAP_Delay: 500 us = 0x01F4
    const uint8_t req[3U] = {xBRIDGE_DAP_ID_DELAY, 0xF4U, 0x01U};
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_dap_delay_us_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(500U, mock_dap_delay_us_fake.arg1_val);
}

void dap_unknown_command_returns_ff_error(void)
{
    const uint8_t req[1U] = {0xEEU}; // unknown command
    xRETURN_t ret = xBRIDGE_DAP_On_USB_Receive(&g_ctx, req, sizeof(req));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
    // Response byte 1 should be 0xFF (error indicator per CMSIS-DAP spec)
    TEST_ASSERT_EQUAL_UINT8(0xFFU, ((const uint8_t *)mock_usb_send_fake.arg1_val)[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(dap_init_succeeds_with_valid_ops);
    RUN_TEST(dap_info_returns_capabilities_byte);
    RUN_TEST(dap_connect_swd_sets_connect_mode);
    RUN_TEST(dap_disconnect_clears_connect_mode);
    RUN_TEST(dap_swj_clock_calls_peripheral_clock);
    RUN_TEST(dap_swj_sequence_calls_peripheral_sequence);
    RUN_TEST(dap_reset_target_calls_peripheral_reset);
    RUN_TEST(dap_delay_calls_peripheral_delay_us);
    RUN_TEST(dap_unknown_command_returns_ff_error);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
