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

// @file test_xbridge_i2c.c
// @brief Host unit tests for the xBRIDGE I2C bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_i2c.h"
#include "xbridge_core.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_I2C_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void i2c_init_succeeds_with_valid_ops(void);
void i2c_write_command_parses_addr_and_data(void);
void i2c_read_command_calls_peripheral_read(void);
void i2c_write_read_calls_peripheral_write_read(void);
void i2c_scan_iterates_7bit_address_range(void);
void i2c_set_speed_calls_peripheral_set_speed(void);
void i2c_frame_too_large_returns_error(void);
void i2c_unknown_cmd_returns_error_response(void);
void i2c_peripheral_fail_returns_error_response(void);

// Helpers /////////////////////////////////////////////////////////////////////////

static void build_write_frame(uint8_t *buf, uint32_t *len, uint32_t seq, uint16_t addr, const uint8_t *data, uint16_t data_len)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_WRITE;
    hdr.seq = seq;
    hdr.length = (uint16_t)(4U + data_len);

    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = (uint8_t)(addr & 0xFFU);
    buf[off++] = 0x00U; // flags
    buf[off++] = (uint8_t)(data_len & 0xFFU);
    buf[off++] = (uint8_t)((data_len >> 8U) & 0xFFU);
    (void)memcpy(buf + off, data, data_len);
    *len = off + data_len;
}

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_I2C_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_i2c_ops, NULL);
}

void tearDown(void)
{
}

void i2c_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_I2C_Context_t ctx;
    xRETURN_t ret = xBRIDGE_I2C_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_i2c_ops, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
}

void i2c_write_command_parses_addr_and_data(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    uint8_t payload[2U] = {0xAAU, 0xBBU};

    build_write_frame(buf, &len, 1U, 0x50U, payload, sizeof(payload));

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_write_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0x50U, mock_i2c_write_fake.arg1_val);
}

void i2c_read_command_calls_peripheral_read(void)
{
    uint8_t buf[32U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_READ;
    hdr.seq = 2U;
    hdr.length = 4U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0x48U; // addr
    buf[sizeof(hdr) + 1U] = 0x00U; // flags
    buf[sizeof(hdr) + 2U] = 0x04U; // read len low
    buf[sizeof(hdr) + 3U] = 0x00U; // read len high

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(hdr) + 4U);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_read_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0x48U, mock_i2c_read_fake.arg1_val);
}

void i2c_write_read_calls_peripheral_write_read(void)
{
    uint8_t buf[32U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_WRITE_READ;
    hdr.seq = 3U;
    hdr.length = 8U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0x68U; // addr
    buf[sizeof(hdr) + 1U] = 0x00U; // flags
    buf[sizeof(hdr) + 2U] = 0x02U; // wlen low
    buf[sizeof(hdr) + 3U] = 0x00U; // wlen high
    buf[sizeof(hdr) + 4U] = 0x02U; // rlen low
    buf[sizeof(hdr) + 5U] = 0x00U; // rlen high
    buf[sizeof(hdr) + 6U] = 0x00U; // wdata[0]
    buf[sizeof(hdr) + 7U] = 0x01U; // wdata[1]

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(hdr) + 8U);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_write_read_fake.call_count);
}

void i2c_scan_iterates_7bit_address_range(void)
{
    uint8_t buf[32U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_SCAN;
    hdr.seq = 4U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    // One device responds at address 0x50
    mock_i2c_read_fake.return_val = xRETURN_xERR_xBRIDGE_PERIPHERAL_FAIL; // default: NACK
    // We can't easily make only addr 0x50 respond with this simple fake; just verify scan calls read 127 times
    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(127U, mock_i2c_read_fake.call_count);
}

void i2c_set_speed_calls_peripheral_set_speed(void)
{
    uint8_t buf[32U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_SET_SPEED;
    hdr.seq = 5U;
    hdr.length = 4U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0x40U;
    buf[sizeof(hdr) + 1U] = 0x42U;
    buf[sizeof(hdr) + 2U] = 0x0FU;
    buf[sizeof(hdr) + 3U] = 0x00U; // 0x000F4240 = 1000000 Hz

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(hdr) + 4U);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_i2c_set_speed_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1000000U, mock_i2c_set_speed_fake.arg1_val);
}

void i2c_frame_too_large_returns_error(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = xBRIDGE_I2C_CMD_WRITE;
    hdr.seq = 6U;
    hdr.length = (uint16_t)(xBRIDGE_MAX_PAYLOAD_BYTES + 1U); // too large
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, ret);
}

void i2c_unknown_cmd_returns_error_response(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_I2C;
    hdr.cmd = 0xFFU; // unknown
    hdr.seq = 7U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    // usb_ops->send should have been called with an error response
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void i2c_peripheral_fail_returns_error_response(void)
{
    mock_i2c_write_fake.return_val = xRETURN_xERR_xBRIDGE_PERIPHERAL_FAIL;

    uint8_t buf[64U];
    uint32_t len = 0U;
    uint8_t payload[2U] = {0xAAU, 0xBBU};

    build_write_frame(buf, &len, 8U, 0x50U, payload, sizeof(payload));

    xRETURN_t ret = xBRIDGE_I2C_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
    // Response status byte should be xBRIDGE_STATUS_ERROR (0x01)
    TEST_ASSERT_EQUAL_UINT8(xBRIDGE_STATUS_ERROR, ((xBRIDGE_Frame_Resp_t *)mock_usb_send_fake.arg1_val)->status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(i2c_init_succeeds_with_valid_ops);
    RUN_TEST(i2c_write_command_parses_addr_and_data);
    RUN_TEST(i2c_read_command_calls_peripheral_read);
    RUN_TEST(i2c_write_read_calls_peripheral_write_read);
    RUN_TEST(i2c_scan_iterates_7bit_address_range);
    RUN_TEST(i2c_set_speed_calls_peripheral_set_speed);
    RUN_TEST(i2c_frame_too_large_returns_error);
    RUN_TEST(i2c_unknown_cmd_returns_error_response);
    RUN_TEST(i2c_peripheral_fail_returns_error_response);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
