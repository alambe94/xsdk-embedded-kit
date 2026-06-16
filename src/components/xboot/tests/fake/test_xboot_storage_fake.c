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

// @file test_xboot_storage_fake.c
// @brief Host tests for xBOOT fake flash storage driver.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_storage_fake.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////
#define TEST_FLASH_SIZE  (256 * 1024U) // 256 KB
#define TEST_SECTOR_SIZE (64 * 1024U)  // 64 KB

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
static uint8_t g_flash_buffer[TEST_FLASH_SIZE];
static xBOOT_Storage_Fake_Context_t g_fake_ctx;
static const xBOOT_Storage_Ops_t *g_ops;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
    g_ops = xBOOT_Storage_Fake_Ops();
    (void)xBOOT_Storage_Fake_Init(&g_fake_ctx, g_flash_buffer, TEST_FLASH_SIZE, TEST_SECTOR_SIZE);
}

void tearDown(void)
{
    g_ops = NULL;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void test_fake_storage_init_rejects_invalid_args(void)
{
    xBOOT_Storage_Fake_Context_t ctx;
    uint8_t buf[100];

    // Null checks
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Storage_Fake_Init(NULL, buf, 100, 10));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER, xBOOT_Storage_Fake_Init(&ctx, NULL, 100, 10));

    // Alignment and size checks
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Fake_Init(&ctx, buf, 0, 10));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Fake_Init(&ctx, buf, 100, 0));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Fake_Init(&ctx, buf, 100, 64)); // Misaligned
}

void test_fake_storage_init_defaults_buffer_to_erased_state(void)
{
    for (uint32_t i = 0U; i < TEST_FLASH_SIZE; i++)
    {
        if (g_flash_buffer[i] != 0xFFU)
        {
            TEST_FAIL_MESSAGE("Buffer not fully initialized to 0xFF");
        }
    }
}

void test_fake_storage_rejects_out_of_bounds_read(void)
{
    uint8_t data[10];

    // Read within limits
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Read(g_ops, &g_fake_ctx, 0, data, 10));

    // Read crossing end boundary
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Read(g_ops, &g_fake_ctx, TEST_FLASH_SIZE - 5U, data, 10));
}

void test_fake_storage_requires_erase_before_write(void)
{
    uint8_t write_data[4] = {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    uint8_t read_data[4];

    // Write to erased flash (all 0xFF) works
    TEST_ASSERT_EQUAL_UINT32(xBOOT_Storage_Write(g_ops, &g_fake_ctx, 0, write_data, 4), xRETURN_xBOOT_OK);
    TEST_ASSERT_EQUAL_UINT32(xBOOT_Storage_Read(g_ops, &g_fake_ctx, 0, read_data, 4), xRETURN_xBOOT_OK);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(write_data, read_data, 4);

    // Overwriting the same location with a different value that flips a '0' back to a '1' fails (requires erase)
    // 0xAA (10101010) -> 0xFF (11111111) flips 0s to 1s. Fails!
    uint8_t bad_data[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_STORAGE_WRITE, xBOOT_Storage_Write(g_ops, &g_fake_ctx, 0, bad_data, 4));

    // Overwriting with value that only clears bits (1s to 0s) is allowed:
    // 0xAA (10101010) & 0x00 (00000000) = 0x00. No bits are flipped from 0 to 1.
    uint8_t ok_data[4] = {0x00U, 0x00U, 0x00U, 0x00U};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Write(g_ops, &g_fake_ctx, 0, ok_data, 4));
}

void test_fake_storage_enforces_sector_alignment_on_erase(void)
{
    // Erase with unaligned offset fails
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Erase(g_ops, &g_fake_ctx, 100, TEST_SECTOR_SIZE));

    // Erase with unaligned length fails
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Storage_Erase(g_ops, &g_fake_ctx, 0, 100));

    // Erase aligned works
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Erase(g_ops, &g_fake_ctx, 0, TEST_SECTOR_SIZE));
}

void test_fake_storage_injects_read_failure(void)
{
    uint8_t data[10];

    xBOOT_Storage_Fake_Inject_Read_Error(&g_fake_ctx, true);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_STORAGE_READ, xBOOT_Storage_Read(g_ops, &g_fake_ctx, 0, data, 10));

    xBOOT_Storage_Fake_Reset_Errors(&g_fake_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Read(g_ops, &g_fake_ctx, 0, data, 10));
}

void test_fake_storage_injects_write_failure(void)
{
    uint8_t data[4] = {0x11U, 0x22U, 0x33U, 0x44U};

    xBOOT_Storage_Fake_Inject_Write_Error(&g_fake_ctx, true);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_STORAGE_WRITE, xBOOT_Storage_Write(g_ops, &g_fake_ctx, 0, data, 4));

    xBOOT_Storage_Fake_Reset_Errors(&g_fake_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Write(g_ops, &g_fake_ctx, 0, data, 4));
}

void test_fake_storage_injects_erase_failure(void)
{
    xBOOT_Storage_Fake_Inject_Erase_Error(&g_fake_ctx, true);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_STORAGE_ERASE, xBOOT_Storage_Erase(g_ops, &g_fake_ctx, 0, TEST_SECTOR_SIZE));

    xBOOT_Storage_Fake_Reset_Errors(&g_fake_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Storage_Erase(g_ops, &g_fake_ctx, 0, TEST_SECTOR_SIZE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fake_storage_init_rejects_invalid_args);
    RUN_TEST(test_fake_storage_init_defaults_buffer_to_erased_state);
    RUN_TEST(test_fake_storage_rejects_out_of_bounds_read);
    RUN_TEST(test_fake_storage_requires_erase_before_write);
    RUN_TEST(test_fake_storage_enforces_sector_alignment_on_erase);
    RUN_TEST(test_fake_storage_injects_read_failure);
    RUN_TEST(test_fake_storage_injects_write_failure);
    RUN_TEST(test_fake_storage_injects_erase_failure);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
