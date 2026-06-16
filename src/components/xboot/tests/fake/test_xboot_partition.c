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

// @file test_xboot_partition.c
// @brief Host tests for xBOOT partition table parsing and lookup.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES
#include "unity.h"

// MODULE INCLUDES
#include "xboot_partition.h"
#include "xboot_verify.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////
#define TEST_STORAGE_SIZE (2 * 1024 * 1024U) // 2 MB
#define TEST_SECTOR_SIZE  (64 * 1024U)       // 64 KB

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
static uint8_t g_table_buf[sizeof(xBOOT_Partition_Table_t)];

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void helper_fill_valid_table(uint8_t *buf, uint32_t num_entries)
{
    uint32_t magic = xBOOT_PARTITION_MAGIC;
    uint32_t version = xBOOT_PARTITION_VERSION_1;

    (void)memset(buf, 0, sizeof(xBOOT_Partition_Table_t));
    (void)memcpy(&buf[offsetof(xBOOT_Partition_Table_t, magic)], &magic, sizeof(uint32_t));
    (void)memcpy(&buf[offsetof(xBOOT_Partition_Table_t, version)], &version, sizeof(uint32_t));
    (void)memcpy(&buf[offsetof(xBOOT_Partition_Table_t, num_entries)], &num_entries, sizeof(uint32_t));

    for (uint32_t i = 0U; i < num_entries; i++)
    {
        xBOOT_Partition_Entry_t entry;
        entry.type = i + 1U;
        entry.offset = i * TEST_SECTOR_SIZE;
        entry.size = TEST_SECTOR_SIZE;
        entry.flags = 0U;

        uint32_t entry_offset = offsetof(xBOOT_Partition_Table_t, entries) + (i * sizeof(xBOOT_Partition_Entry_t));
        (void)memcpy(&buf[entry_offset], &entry, sizeof(xBOOT_Partition_Entry_t));
    }

    uint32_t crc = xBOOT_CRC32_Calculate(buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));
}

void setUp(void)
{
    (void)memset(g_table_buf, 0, sizeof(g_table_buf));
}

void tearDown(void)
{
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void test_partition_parse_rejects_null_arguments(void)
{
    xBOOT_Partition_Table_t table;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER,
                             xBOOT_Partition_Table_Parse(NULL, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_NULL_POINTER,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, NULL));
}

void test_partition_parse_rejects_insufficient_buffer_size(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    TEST_ASSERT_EQUAL_UINT32(
        xRETURN_xERR_xBOOT_INVALID_ARGUMENT,
        xBOOT_Partition_Table_Parse(g_table_buf, sizeof(xBOOT_Partition_Table_t) - 1U, TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_invalid_storage_parameters(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), 0, TEST_SECTOR_SIZE, &table));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, 0, &table));
}

void test_partition_parse_rejects_bad_magic(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    // Corrupt magic
    uint32_t bad_magic = 0xDEADBEEFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, magic)], &bad_magic, sizeof(uint32_t));

    // Recompute CRC to bypass CRC check
    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_bad_version(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    // Corrupt version
    uint32_t bad_version = 99U;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, version)], &bad_version, sizeof(uint32_t));

    // Recompute CRC to bypass CRC check
    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_bad_crc(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    // Corrupt CRC field directly
    g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)] ^= 0xFFU;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_CRC_MISMATCH,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_out_of_bounds_entries(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 1);

    // Corrupt size of entry 0 to exceed storage size
    uint32_t bad_size = TEST_STORAGE_SIZE + TEST_SECTOR_SIZE;
    uint32_t entry_offset = offsetof(xBOOT_Partition_Table_t, entries) + offsetof(xBOOT_Partition_Entry_t, size);
    (void)memcpy(&g_table_buf[entry_offset], &bad_size, sizeof(uint32_t));

    // Recompute CRC
    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_misaligned_entries(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 1);

    // Corrupt offset of entry 0 to be misaligned (not aligned to 64 KB)
    uint32_t bad_offset = 123U;
    uint32_t entry_offset = offsetof(xBOOT_Partition_Table_t, entries) + offsetof(xBOOT_Partition_Entry_t, offset);
    (void)memcpy(&g_table_buf[entry_offset], &bad_offset, sizeof(uint32_t));

    // Recompute CRC
    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_rejects_overlapping_entries(void)
{
    xBOOT_Partition_Table_t table;
    helper_fill_valid_table(g_table_buf, 2);

    // Corrupt entry 1 offset to overlap with entry 0
    // Entry 0 offset = 0, size = 64 KB
    // Set Entry 1 offset to 32 KB (overlapping, even though unaligned, but let's make it aligned and overlapping):
    // Actually, to keep it aligned, let's make entry 0 offset=0, size=128 KB, and entry 1 offset=64 KB, size=64 KB.
    uint32_t size_0 = TEST_SECTOR_SIZE * 2U;
    uint32_t offset_1 = TEST_SECTOR_SIZE;
    uint32_t size_1 = TEST_SECTOR_SIZE;

    uint32_t entry_0_size_offset = offsetof(xBOOT_Partition_Table_t, entries[0]) + offsetof(xBOOT_Partition_Entry_t, size);
    uint32_t entry_1_off_offset = offsetof(xBOOT_Partition_Table_t, entries[1]) + offsetof(xBOOT_Partition_Entry_t, offset);
    uint32_t entry_1_size_offset = offsetof(xBOOT_Partition_Table_t, entries[1]) + offsetof(xBOOT_Partition_Entry_t, size);

    (void)memcpy(&g_table_buf[entry_0_size_offset], &size_0, sizeof(uint32_t));
    (void)memcpy(&g_table_buf[entry_1_off_offset], &offset_1, sizeof(uint32_t));
    (void)memcpy(&g_table_buf[entry_1_size_offset], &size_1, sizeof(uint32_t));

    // Recompute CRC
    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));
}

void test_partition_parse_success_and_lookups(void)
{
    xBOOT_Partition_Table_t table;
    xBOOT_Partition_Entry_t entry;

    // Build valid table with 2 entries:
    // Entry 0: Type = PRIMARY_APP (2), offset = 0, size = 64 KB
    // Entry 1: Type = SECONDARY_APP (3), offset = 64 KB, size = 64 KB
    uint32_t magic = xBOOT_PARTITION_MAGIC;
    uint32_t version = xBOOT_PARTITION_VERSION_1;
    uint32_t num_entries = 2U;

    (void)memset(g_table_buf, 0, sizeof(g_table_buf));
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, magic)], &magic, sizeof(uint32_t));
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, version)], &version, sizeof(uint32_t));
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, num_entries)], &num_entries, sizeof(uint32_t));

    xBOOT_Partition_Entry_t e0 = {xBOOT_PARTITION_TYPE_PRIMARY_APP, 0U, TEST_SECTOR_SIZE, 0U};
    xBOOT_Partition_Entry_t e1 = {xBOOT_PARTITION_TYPE_SECONDARY_APP, TEST_SECTOR_SIZE, TEST_SECTOR_SIZE, 0U};

    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, entries[0])], &e0, sizeof(xBOOT_Partition_Entry_t));
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, entries[1])], &e1, sizeof(xBOOT_Partition_Entry_t));

    uint32_t crc = xBOOT_CRC32_Calculate(g_table_buf, offsetof(xBOOT_Partition_Table_t, crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    (void)memcpy(&g_table_buf[offsetof(xBOOT_Partition_Table_t, crc32)], &crc, sizeof(uint32_t));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK,
                             xBOOT_Partition_Table_Parse(g_table_buf, sizeof(g_table_buf), TEST_STORAGE_SIZE, TEST_SECTOR_SIZE, &table));

    // Type lookup tests
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Partition_Lookup_Type(&table, xBOOT_PARTITION_TYPE_PRIMARY_APP, &entry));
    TEST_ASSERT_EQUAL_UINT32(0U, entry.offset);
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, entry.size);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Partition_Lookup_Type(&table, xBOOT_PARTITION_TYPE_SECONDARY_APP, &entry));
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, entry.offset);
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, entry.size);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_PARTITION,
                             xBOOT_Partition_Lookup_Type(&table, xBOOT_PARTITION_TYPE_BOOTLOADER, &entry));

    // Slot lookup tests
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Partition_Lookup_Slot(&table, 0U, &entry));
    TEST_ASSERT_EQUAL_UINT32(0U, entry.offset);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xBOOT_OK, xBOOT_Partition_Lookup_Slot(&table, 1U, &entry));
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, entry.offset);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xBOOT_INVALID_ARGUMENT, xBOOT_Partition_Lookup_Slot(&table, 2U, &entry));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_partition_parse_rejects_null_arguments);
    RUN_TEST(test_partition_parse_rejects_insufficient_buffer_size);
    RUN_TEST(test_partition_parse_rejects_invalid_storage_parameters);
    RUN_TEST(test_partition_parse_rejects_bad_magic);
    RUN_TEST(test_partition_parse_rejects_bad_version);
    RUN_TEST(test_partition_parse_rejects_bad_crc);
    RUN_TEST(test_partition_parse_rejects_out_of_bounds_entries);
    RUN_TEST(test_partition_parse_rejects_misaligned_entries);
    RUN_TEST(test_partition_parse_rejects_overlapping_entries);
    RUN_TEST(test_partition_parse_success_and_lookups);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
