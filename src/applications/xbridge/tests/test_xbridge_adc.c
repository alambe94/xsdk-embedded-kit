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

// @file test_xbridge_adc.c
// @brief Host unit tests for the xBRIDGE ADC bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_adc.h"
#include "xbridge_core.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_ADC_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void adc_init_succeeds_with_valid_ops(void);
void adc_init_sets_default_resolution(void);
void adc_read_single_calls_peripheral_and_returns_result(void);
void adc_read_multi_calls_peripheral_with_mask(void);
void adc_set_resolution_valid_values(void);
void adc_set_resolution_rejects_invalid_value(void);
void adc_set_reference_calls_peripheral(void);
void adc_set_sample_rate_calls_peripheral(void);
void adc_unknown_cmd_returns_error_response(void);

// Helpers /////////////////////////////////////////////////////////////////////////

static void build_frame_u32(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint32_t value)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_ADC;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 4U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(value & 0xFFU);
    buf[off++] = (uint8_t)((value >> 8U) & 0xFFU);
    buf[off++] = (uint8_t)((value >> 16U) & 0xFFU);
    buf[off++] = (uint8_t)((value >> 24U) & 0xFFU);
    *len = off;
}

static void build_frame_u8(uint8_t *buf, uint32_t *len, uint8_t cmd_id, uint32_t seq, uint8_t value)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_ADC;
    hdr.cmd = cmd_id;
    hdr.seq = seq;
    hdr.length = 1U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr)] = value;
    *len = sizeof(hdr) + 1U;
}

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_ADC_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_adc_ops, NULL);
}

void tearDown(void)
{
}

void adc_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_ADC_Context_t ctx;
    TEST_ASSERT_EQUAL(xRETURN_OK, xBRIDGE_ADC_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_adc_ops, NULL));
}

void adc_init_sets_default_resolution(void)
{
    TEST_ASSERT_EQUAL_UINT8(12U, g_ctx.resolution_bits);
}

void adc_read_single_calls_peripheral_and_returns_result(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_u32(buf, &len, xBRIDGE_ADC_CMD_READ_SINGLE, 1U, 3U); // channel 3

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_adc_read_single_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(3U, mock_adc_read_single_fake.arg1_val); // channel
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
    // Response should have 4 bytes of data
    TEST_ASSERT_EQUAL_UINT16(4U, ((xBRIDGE_Frame_Resp_t *)mock_usb_send_fake.arg1_val)->length);
}

void adc_read_multi_calls_peripheral_with_mask(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_u32(buf, &len, xBRIDGE_ADC_CMD_READ_MULTI, 2U, 0x07U); // channels 0,1,2

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_adc_read_multi_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0x07U, mock_adc_read_multi_fake.arg1_val); // mask
    TEST_ASSERT_EQUAL_UINT32(3U, mock_adc_read_multi_fake.arg3_val);    // count = popcount(0x07) = 3
}

void adc_set_resolution_valid_values(void)
{
    const uint8_t valid_bits[4U] = {8U, 10U, 12U, 16U};

    for (uint32_t i = 0U; i < 4U; i++)
    {
        reset_all_mocks();
        uint8_t buf[64U];
        uint32_t len = 0U;
        build_frame_u8(buf, &len, xBRIDGE_ADC_CMD_SET_RESOLUTION, i, valid_bits[i]);

        xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
        TEST_ASSERT_EQUAL(xRETURN_OK, ret);
        TEST_ASSERT_EQUAL_UINT32(1U, mock_adc_set_resolution_fake.call_count);
        TEST_ASSERT_EQUAL_UINT8(valid_bits[i], mock_adc_set_resolution_fake.arg1_val);
    }
}

void adc_set_resolution_rejects_invalid_value(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_u8(buf, &len, xBRIDGE_ADC_CMD_SET_RESOLUTION, 5U, 14U); // 14-bit is invalid

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_adc_set_resolution_fake.call_count);
    // Error status response should be sent
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void adc_set_reference_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_u8(buf, &len, xBRIDGE_ADC_CMD_SET_REFERENCE, 6U, xBRIDGE_ADC_REF_INTERNAL);

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_adc_set_reference_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_ADC_REF_INTERNAL, mock_adc_set_reference_fake.arg1_val);
}

void adc_set_sample_rate_calls_peripheral(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    build_frame_u32(buf, &len, xBRIDGE_ADC_CMD_SET_SAMPLE_RATE, 7U, 100000U);

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_adc_set_sample_rate_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(100000U, mock_adc_set_sample_rate_fake.arg1_val);
}

void adc_unknown_cmd_returns_error_response(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_ADC;
    hdr.cmd = 0xFFU;
    hdr.seq = 8U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_ADC_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(adc_init_succeeds_with_valid_ops);
    RUN_TEST(adc_init_sets_default_resolution);
    RUN_TEST(adc_read_single_calls_peripheral_and_returns_result);
    RUN_TEST(adc_read_multi_calls_peripheral_with_mask);
    RUN_TEST(adc_set_resolution_valid_values);
    RUN_TEST(adc_set_resolution_rejects_invalid_value);
    RUN_TEST(adc_set_reference_calls_peripheral);
    RUN_TEST(adc_set_sample_rate_calls_peripheral);
    RUN_TEST(adc_unknown_cmd_returns_error_response);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
