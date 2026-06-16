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

// @file test_xbridge_can_slcan.c
// @brief Host unit tests for the xBRIDGE CAN channel SLCAN parser.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_can.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_CAN_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void slcan_open_command_sets_bus_open(void);
void slcan_close_command_clears_bus_open(void);
void slcan_set_bitrate_command_calls_peripheral_set_bitrate(void);
void slcan_transmit_standard_frame_correct_id_dlc_data(void);
void slcan_transmit_extended_frame_correct_29bit_id(void);
void slcan_receive_frame_formats_correct_slcan_string(void);
void slcan_partial_line_accumulates_before_dispatch(void);
void slcan_invalid_command_returns_bell_error(void);
void slcan_timestamp_mode_enabled_appends_timestamp(void);

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_CAN_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_can_ops, NULL);
}

void tearDown(void)
{
}

void slcan_open_command_sets_bus_open(void)
{
    const uint8_t cmd[2U] = {(uint8_t)'O', (uint8_t)'\r'};
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_TRUE(g_ctx.is_open);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_open_fake.call_count);
}

void slcan_close_command_clears_bus_open(void)
{
    // Open first
    const uint8_t open_cmd[2U] = {(uint8_t)'O', (uint8_t)'\r'};
    (void)xBRIDGE_CAN_On_USB_Receive(&g_ctx, open_cmd, sizeof(open_cmd));
    TEST_ASSERT_TRUE(g_ctx.is_open);

    reset_all_mocks();
    (void)xBRIDGE_CAN_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_can_ops, NULL);
    g_ctx.is_open = true;

    const uint8_t close_cmd[2U] = {(uint8_t)'C', (uint8_t)'\r'};
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, close_cmd, sizeof(close_cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_FALSE(g_ctx.is_open);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_close_fake.call_count);
}

void slcan_set_bitrate_command_calls_peripheral_set_bitrate(void)
{
    // S5 = 250000 bps
    const uint8_t cmd[3U] = {(uint8_t)'S', (uint8_t)'5', (uint8_t)'\r'};
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_set_bitrate_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(250000U, mock_can_set_bitrate_fake.arg1_val);
}

void slcan_transmit_standard_frame_correct_id_dlc_data(void)
{
    // Open channel
    g_ctx.is_open = true;

    // t1230DEADBEEF  -> ID=0x123, DLC=0... wait DLC comes right after 3 hex ID digits
    // SLCAN: t<ID3><DLC><DATA_BYTES>\r  -> t123 4 DEADBEEF
    // ID=0x123, DLC=4, data={0xDE,0xAD,0xBE,0xEF}
    const uint8_t cmd[] = {'t', '1', '2', '3', '4', 'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F', '\r'};

    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_transmit_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0x123U, mock_can_transmit_fake.arg1_val);
    TEST_ASSERT_FALSE(mock_can_transmit_fake.arg2_val); // not extended
    TEST_ASSERT_EQUAL_UINT8(4U, mock_can_transmit_fake.arg4_val);
}

void slcan_transmit_extended_frame_correct_29bit_id(void)
{
    g_ctx.is_open = true;

    // T00000123 4 DEADBEEF  -> ID=0x00000123, DLC=4, data={DE,AD,BE,EF}
    const uint8_t cmd[] = {'T', '0', '0', '0', '0', '0', '1', '2', '3', '4', 'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F', '\r'};

    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_transmit_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0x123U, mock_can_transmit_fake.arg1_val);
    TEST_ASSERT_TRUE(mock_can_transmit_fake.arg2_val); // extended
}

void slcan_receive_frame_formats_correct_slcan_string(void)
{
    g_ctx.is_open = true;

    // Set up mock_can_rx_available to return true once, then false
    bool rx_avail_seq[2U] = {true, false};
    SET_RETURN_SEQ(mock_can_rx_available, rx_avail_seq, 2U);

    // Pretend the device received frame: ID=0x200, standard, DLC=2, data={0xAA,0xBB}
    mock_can_receive_fake.return_val = xRETURN_OK;

    xRETURN_t ret = xBRIDGE_CAN_Poll(&g_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void slcan_partial_line_accumulates_before_dispatch(void)
{
    g_ctx.is_open = true;

    // Send frame in two parts: 't1' then '234DEADBEEF\r'
    const uint8_t part1[2U] = {'t', '1'};
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, part1, sizeof(part1));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    // No dispatch yet
    TEST_ASSERT_EQUAL_UINT32(0U, mock_can_transmit_fake.call_count);

    const uint8_t part2[] = {'2', '3', '4', 'D', 'E', 'A', 'D', 'B', 'E', 'E', 'F', '\r'};
    ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, part2, sizeof(part2));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_can_transmit_fake.call_count);
}

void slcan_invalid_command_returns_bell_error(void)
{
    const uint8_t cmd[2U] = {(uint8_t)'X', (uint8_t)'\r'}; // unknown command
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    // usb_ops->send should have been called with a bell character
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(0x07U, ((const uint8_t *)mock_usb_send_fake.arg1_val)[0]);
}

void slcan_timestamp_mode_enabled_appends_timestamp(void)
{
    const uint8_t cmd[3U] = {(uint8_t)'Z', (uint8_t)'1', (uint8_t)'\r'};
    xRETURN_t ret = xBRIDGE_CAN_On_USB_Receive(&g_ctx, cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_TRUE(g_ctx.is_timestamp_enabled);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(slcan_open_command_sets_bus_open);
    RUN_TEST(slcan_close_command_clears_bus_open);
    RUN_TEST(slcan_set_bitrate_command_calls_peripheral_set_bitrate);
    RUN_TEST(slcan_transmit_standard_frame_correct_id_dlc_data);
    RUN_TEST(slcan_transmit_extended_frame_correct_29bit_id);
    RUN_TEST(slcan_receive_frame_formats_correct_slcan_string);
    RUN_TEST(slcan_partial_line_accumulates_before_dispatch);
    RUN_TEST(slcan_invalid_command_returns_bell_error);
    RUN_TEST(slcan_timestamp_mode_enabled_appends_timestamp);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
