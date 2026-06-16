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

// @file test_xbridge_pwm.c
// @brief Host unit tests for the xBRIDGE PWM bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_pwm.h"
#include "xbridge_core.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_PWM_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void pwm_init_succeeds_with_valid_ops(void);
void pwm_set_frequency_calls_peripheral(void);
void pwm_set_duty_calls_peripheral(void);
void pwm_set_duty_rejects_out_of_range(void);
void pwm_enable_calls_peripheral(void);
void pwm_disable_calls_peripheral(void);
void pwm_set_polarity_calls_peripheral(void);
void pwm_unknown_cmd_returns_error_response(void);

// Helpers /////////////////////////////////////////////////////////////////////////

static void build_frame_channel_u32(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint32_t channel, uint32_t value)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_PWM;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 8U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(channel & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 24U) & 0xFFU);
    buf[off++] = (uint8_t)(value & 0xFFU);
    buf[off++] = (uint8_t)((value >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((value >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((value >> 24U) & 0xFFU);
    *len = off;
}

static void build_frame_channel_only(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint32_t channel)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_PWM;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 4U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(channel & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((channel >> 24U) & 0xFFU);
    *len = off;
}

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_PWM_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_pwm_ops, NULL);
}

void tearDown(void)
{
}

void pwm_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_PWM_Context_t ctx;
    TEST_ASSERT_EQUAL(xRETURN_OK, xBRIDGE_PWM_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_pwm_ops, NULL));
}

void pwm_set_frequency_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_channel_u32(buf, &len, xBRIDGE_PWM_CMD_SET_FREQUENCY, 1U, 0U, 1000U);

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_set_frequency_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_pwm_set_frequency_fake.arg1_val);    // channel
    TEST_ASSERT_EQUAL_UINT32(1000U, mock_pwm_set_frequency_fake.arg2_val); // Hz
}

void pwm_set_duty_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_channel_u32(buf, &len, xBRIDGE_PWM_CMD_SET_DUTY, 2U, 1U, 5000U);

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_set_duty_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_set_duty_fake.arg1_val);    // channel
    TEST_ASSERT_EQUAL_UINT32(5000U, mock_pwm_set_duty_fake.arg2_val); // 50.00%
}

void pwm_set_duty_rejects_out_of_range(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_channel_u32(buf, &len, xBRIDGE_PWM_CMD_SET_DUTY, 3U, 0U, 10001U);

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_pwm_set_duty_fake.call_count);
    // An error status response should have been sent
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void pwm_enable_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_channel_only(buf, &len, xBRIDGE_PWM_CMD_ENABLE, 4U, 2U);

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_enable_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(2U, mock_pwm_enable_fake.arg1_val); // channel
}

void pwm_disable_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_channel_only(buf, &len, xBRIDGE_PWM_CMD_DISABLE, 5U, 2U);

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_disable_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(2U, mock_pwm_disable_fake.arg1_val);
}

void pwm_set_polarity_calls_peripheral(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t) + 5U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_PWM;
    hdr.cmd = xBRIDGE_PWM_CMD_SET_POLARITY;
    hdr.seq = 6U;
    hdr.length = 5U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0U; // channel low byte
    buf[sizeof(hdr) + 1U] = 0U;
    buf[sizeof(hdr) + 2U] = 0U;
    buf[sizeof(hdr) + 3U] = 0U; // channel = 0
    buf[sizeof(hdr) + 4U] = xBRIDGE_PWM_POLARITY_INVERTED;

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_pwm_set_polarity_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_PWM_POLARITY_INVERTED, mock_pwm_set_polarity_fake.arg2_val);
}

void pwm_unknown_cmd_returns_error_response(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_PWM;
    hdr.cmd = 0xFFU;
    hdr.seq = 7U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_PWM_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(pwm_init_succeeds_with_valid_ops);
    RUN_TEST(pwm_set_frequency_calls_peripheral);
    RUN_TEST(pwm_set_duty_calls_peripheral);
    RUN_TEST(pwm_set_duty_rejects_out_of_range);
    RUN_TEST(pwm_enable_calls_peripheral);
    RUN_TEST(pwm_disable_calls_peripheral);
    RUN_TEST(pwm_set_polarity_calls_peripheral);
    RUN_TEST(pwm_unknown_cmd_returns_error_response);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
