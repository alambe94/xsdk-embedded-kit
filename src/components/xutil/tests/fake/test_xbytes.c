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

// @file test_xbytes.c
// @brief Host tests for byte-order and byte-manipulation helpers.

#include <stdint.h>

#include "unity.h"

#include "xbytes.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_read_little_endian_values_from_byte_buffer(void)
{
    const uint8_t buf[] = {0x78U, 0x56U, 0x34U, 0x12U};

    TEST_ASSERT_EQUAL_UINT16(0x5678U, xRead_LE16(&buf[0U]));
    TEST_ASSERT_EQUAL_UINT32(0x12345678UL, xRead_LE32(&buf[0U]));
}

void test_write_little_endian_values_to_byte_buffer(void)
{
    uint8_t buf[4U] = {0U, 0U, 0U, 0U};

    xWrite_LE16(buf, 0xABCDU);
    TEST_ASSERT_EQUAL_UINT8(0xCDU, buf[0U]);
    TEST_ASSERT_EQUAL_UINT8(0xABU, buf[1U]);

    xWrite_LE32(buf, 0x12345678UL);
    TEST_ASSERT_EQUAL_UINT8(0x78U, buf[0U]);
    TEST_ASSERT_EQUAL_UINT8(0x56U, buf[1U]);
    TEST_ASSERT_EQUAL_UINT8(0x34U, buf[2U]);
    TEST_ASSERT_EQUAL_UINT8(0x12U, buf[3U]);
}

void test_read_big_endian_values_from_byte_buffer(void)
{
    const uint8_t buf[] = {0x12U, 0x34U, 0x56U, 0x78U};

    TEST_ASSERT_EQUAL_UINT16(0x1234U, xRead_BE16(&buf[0U]));
    TEST_ASSERT_EQUAL_UINT32(0x12345678UL, xRead_BE32(&buf[0U]));
}

void test_write_big_endian_values_to_byte_buffer(void)
{
    uint8_t buf[4U] = {0U, 0U, 0U, 0U};

    xWrite_BE16(buf, 0xABCDU);
    TEST_ASSERT_EQUAL_UINT8(0xABU, buf[0U]);
    TEST_ASSERT_EQUAL_UINT8(0xCDU, buf[1U]);

    xWrite_BE32(buf, 0x12345678UL);
    TEST_ASSERT_EQUAL_UINT8(0x12U, buf[0U]);
    TEST_ASSERT_EQUAL_UINT8(0x34U, buf[1U]);
    TEST_ASSERT_EQUAL_UINT8(0x56U, buf[2U]);
    TEST_ASSERT_EQUAL_UINT8(0x78U, buf[3U]);
}

void test_extract_bytes_from_cpu_order_values(void)
{
    TEST_ASSERT_EQUAL_UINT8(0xCDU, xU16_LOW_BYTE(0xABCDU));
    TEST_ASSERT_EQUAL_UINT8(0xABU, xU16_HIGH_BYTE(0xABCDU));

    TEST_ASSERT_EQUAL_UINT8(0x78U, xU32_BYTE0(0x12345678UL));
    TEST_ASSERT_EQUAL_UINT8(0x56U, xU32_BYTE1(0x12345678UL));
    TEST_ASSERT_EQUAL_UINT8(0x34U, xU32_BYTE2(0x12345678UL));
    TEST_ASSERT_EQUAL_UINT8(0x12U, xU32_BYTE3(0x12345678UL));
}

void test_extract_bytes_after_little_endian_conversion(void)
{
    uint16_t le16 = xCPU_TO_LE16(0xABCDU);
    uint32_t le32 = xCPU_TO_LE32(0x12345678UL);
    uint16_t value16 = xLE16_TO_CPU(le16);
    uint32_t value32 = xLE32_TO_CPU(le32);

    TEST_ASSERT_EQUAL_UINT8(0xCDU, xU16_LOW_BYTE(value16));
    TEST_ASSERT_EQUAL_UINT8(0xABU, xU16_HIGH_BYTE(value16));

    TEST_ASSERT_EQUAL_UINT8(0x78U, xU32_BYTE0(value32));
    TEST_ASSERT_EQUAL_UINT8(0x56U, xU32_BYTE1(value32));
    TEST_ASSERT_EQUAL_UINT8(0x34U, xU32_BYTE2(value32));
    TEST_ASSERT_EQUAL_UINT8(0x12U, xU32_BYTE3(value32));
}

void test_make_values_from_little_endian_ordered_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT16(0xABCDU, xMAKE_U16(0xCDU, 0xABU));
    TEST_ASSERT_EQUAL_UINT32(0x12345678UL, xMAKE_U32(0x78U, 0x56U, 0x34U, 0x12U));
}

void test_read_and_write_helpers_work_at_unaligned_offsets(void)
{
    uint8_t buf[6U] = {0U, 0U, 0U, 0U, 0U, 0U};

    xWrite_LE32(&buf[1U], 0xA1B2C3D4UL);
    TEST_ASSERT_EQUAL_UINT32(0xA1B2C3D4UL, xRead_LE32(&buf[1U]));

    xWrite_BE16(&buf[2U], 0x5A6BU);
    TEST_ASSERT_EQUAL_UINT16(0x5A6BU, xRead_BE16(&buf[2U]));
}

void test_convert_little_endian_values_round_trip(void)
{
    uint16_t value16 = 0xBEEFU;
    uint32_t value32 = 0xDEADBEEFUL;

    TEST_ASSERT_EQUAL_UINT16(value16, xLE16_TO_CPU(xCPU_TO_LE16(value16)));
    TEST_ASSERT_EQUAL_UINT32(value32, xLE32_TO_CPU(xCPU_TO_LE32(value32)));
}

void test_convert_big_endian_values_round_trip(void)
{
    uint16_t value16 = 0xBEEFU;
    uint32_t value32 = 0xDEADBEEFUL;

    TEST_ASSERT_EQUAL_UINT16(value16, xBE16_TO_CPU(xCPU_TO_BE16(value16)));
    TEST_ASSERT_EQUAL_UINT32(value32, xBE32_TO_CPU(xCPU_TO_BE32(value32)));
}

void test_swap_values(void)
{
    TEST_ASSERT_EQUAL_UINT16(0x3412U, xSWAP_U16(0x1234U));
    TEST_ASSERT_EQUAL_UINT32(0x78563412UL, xSWAP_U32(0x12345678UL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_little_endian_values_from_byte_buffer);
    RUN_TEST(test_write_little_endian_values_to_byte_buffer);
    RUN_TEST(test_read_big_endian_values_from_byte_buffer);
    RUN_TEST(test_write_big_endian_values_to_byte_buffer);
    RUN_TEST(test_extract_bytes_from_cpu_order_values);
    RUN_TEST(test_extract_bytes_after_little_endian_conversion);
    RUN_TEST(test_make_values_from_little_endian_ordered_bytes);
    RUN_TEST(test_read_and_write_helpers_work_at_unaligned_offsets);
    RUN_TEST(test_convert_little_endian_values_round_trip);
    RUN_TEST(test_convert_big_endian_values_round_trip);
    RUN_TEST(test_swap_values);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
