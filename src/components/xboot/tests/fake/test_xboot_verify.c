// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file test_xboot_verify.c
// @brief Host tests for xBOOT CRC32 calculation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_verify.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
}

void tearDown(void)
{
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void test_crc32_empty_buffer(void)
{
    // Empty buffer should result in 0 after XORing
    uint32_t raw_crc = xBOOT_CRC32_Calculate(NULL, 0, xBOOT_CRC32_INIT);
    TEST_ASSERT_EQUAL_UINT32(0U, raw_crc ^ 0xFFFFFFFFU);
}

void test_crc32_standard_check_sequence(void)
{
    // CRC-32 of "123456789" is standardly 0xCBF43926
    const uint8_t data[] = "123456789";
    uint32_t raw_crc = xBOOT_CRC32_Calculate(data, 9, xBOOT_CRC32_INIT);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926U, raw_crc ^ 0xFFFFFFFFU);
}

void test_crc32_hello_world(void)
{
    // CRC-32 of "Hello, World!" is 0xEC4AC3D0
    const uint8_t data[] = "Hello, World!";
    uint32_t raw_crc = xBOOT_CRC32_Calculate(data, 13, xBOOT_CRC32_INIT);
    TEST_ASSERT_EQUAL_UINT32(0xEC4AC3D0U, raw_crc ^ 0xFFFFFFFFU);
}

void test_crc32_chunked(void)
{
    // Test that splitting calculation into chunks yields the same result
    const uint8_t data[] = "123456789";
    uint32_t raw_crc = xBOOT_CRC32_Calculate(&data[0], 3, xBOOT_CRC32_INIT);
    raw_crc = xBOOT_CRC32_Calculate(&data[3], 3, raw_crc);
    raw_crc = xBOOT_CRC32_Calculate(&data[6], 3, raw_crc);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926U, raw_crc ^ 0xFFFFFFFFU);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_empty_buffer);
    RUN_TEST(test_crc32_standard_check_sequence);
    RUN_TEST(test_crc32_hello_world);
    RUN_TEST(test_crc32_chunked);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
