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

// @file test_xbridge_spi.c
// @brief Host unit tests for the xBRIDGE SPI bridge channel.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xbridge_spi.h"
#include "xbridge_core.h"
#include "test_helpers.h"

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static xBRIDGE_SPI_Context_t g_ctx;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

void setUp(void);
void tearDown(void);

void spi_init_succeeds_with_valid_ops(void);
void spi_transfer_asserts_cs_and_calls_transfer(void);
void spi_cs_assert_calls_peripheral(void);
void spi_cs_deassert_calls_peripheral(void);
void spi_set_mode_calls_peripheral(void);
void spi_set_speed_calls_peripheral(void);
void spi_unknown_cmd_returns_error_response(void);
void spi_transfer_keep_cs_flag_skips_deassert(void);

// Helpers /////////////////////////////////////////////////////////////////////////

static void
build_transfer_frame(uint8_t *buf, uint32_t *len, uint32_t seq, uint8_t cs_idx, uint8_t flags, const uint8_t *mosi, uint16_t data_len)
{
    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = xBRIDGE_SPI_CMD_TRANSFER;
    hdr.seq = seq;
    hdr.length = (uint16_t)(4U + data_len);

    (void)memcpy(buf, &hdr, sizeof(hdr));
    uint32_t off = sizeof(hdr);
    buf[off++] = cs_idx;
    buf[off++] = flags;
    buf[off++] = (uint8_t)(data_len & 0xFFU);
    buf[off++] = (uint8_t)((data_len >> 8U) & 0xFFU);
    (void)memcpy(buf + off, mosi, data_len);
    *len = off + data_len;
}

// IMPLEMENTATION //////////////////////////////////////////////////////////////////

void setUp(void)
{
    reset_all_mocks();
    (void)memset(&g_ctx, 0, sizeof(g_ctx));
    (void)xBRIDGE_SPI_Init(&g_ctx, &g_mock_usb_ops, NULL, &g_mock_spi_ops, NULL);
}

void tearDown(void)
{
}

void spi_init_succeeds_with_valid_ops(void)
{
    xBRIDGE_SPI_Context_t ctx;
    xRETURN_t ret = xBRIDGE_SPI_Init(&ctx, &g_mock_usb_ops, NULL, &g_mock_spi_ops, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
}

void spi_transfer_asserts_cs_and_calls_transfer(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    const uint8_t mosi[4U] = {0x9FU, 0x00U, 0x00U, 0x00U};

    build_transfer_frame(buf, &len, 1U, 0U, 0U, mosi, sizeof(mosi));

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_cs_assert_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_transfer_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_cs_deassert_fake.call_count);
}

void spi_cs_assert_calls_peripheral(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t) + 1U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = xBRIDGE_SPI_CMD_CS_ASSERT;
    hdr.seq = 2U;
    hdr.length = 1U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr)] = 0U; // cs_idx

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_cs_assert_fake.call_count);
}

void spi_cs_deassert_calls_peripheral(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t) + 1U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = xBRIDGE_SPI_CMD_CS_DEASSERT;
    hdr.seq = 3U;
    hdr.length = 1U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr)] = 0U; // cs_idx

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_cs_deassert_fake.call_count);
}

void spi_set_mode_calls_peripheral(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t) + 2U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = xBRIDGE_SPI_CMD_SET_MODE;
    hdr.seq = 4U;
    hdr.length = 2U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0U; // cpol
    buf[sizeof(hdr) + 1U] = 0U; // cpha

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_set_mode_fake.call_count);
}

void spi_set_speed_calls_peripheral(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t) + 4U];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = xBRIDGE_SPI_CMD_SET_SPEED;
    hdr.seq = 5U;
    hdr.length = 4U;
    (void)memcpy(buf, &hdr, sizeof(hdr));
    buf[sizeof(hdr) + 0U] = 0x80U;
    buf[sizeof(hdr) + 1U] = 0x84U;
    buf[sizeof(hdr) + 2U] = 0x1EU;
    buf[sizeof(hdr) + 3U] = 0x00U; // 0x001E8480 = 2000000 Hz

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_set_speed_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(2000000U, mock_spi_set_speed_fake.arg1_val);
}

void spi_unknown_cmd_returns_error_response(void)
{
    uint8_t buf[sizeof(xBRIDGE_Frame_Cmd_t)];
    (void)memset(buf, 0, sizeof(buf));

    xBRIDGE_Frame_Cmd_t hdr;
    hdr.channel = xBRIDGE_CHANNEL_SPI;
    hdr.cmd = 0xFFU; // unknown
    hdr.seq = 6U;
    hdr.length = 0U;
    (void)memcpy(buf, &hdr, sizeof(hdr));

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, sizeof(hdr));
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_usb_send_fake.call_count);
}

void spi_transfer_keep_cs_flag_skips_deassert(void)
{
    uint8_t buf[64U];
    uint32_t len = 0U;
    const uint8_t mosi[2U] = {0x01U, 0x02U};

    build_transfer_frame(buf, &len, 7U, 0U, xBRIDGE_SPI_FLAG_KEEP_CS, mosi, sizeof(mosi));

    xRETURN_t ret = xBRIDGE_SPI_On_USB_Receive(&g_ctx, buf, len);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_cs_assert_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, mock_spi_transfer_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_spi_cs_deassert_fake.call_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(spi_init_succeeds_with_valid_ops);
    RUN_TEST(spi_transfer_asserts_cs_and_calls_transfer);
    RUN_TEST(spi_cs_assert_calls_peripheral);
    RUN_TEST(spi_cs_deassert_calls_peripheral);
    RUN_TEST(spi_set_mode_calls_peripheral);
    RUN_TEST(spi_set_speed_calls_peripheral);
    RUN_TEST(spi_unknown_cmd_returns_error_response);
    RUN_TEST(spi_transfer_keep_cs_flag_skips_deassert);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
