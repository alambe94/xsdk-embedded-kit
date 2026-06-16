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

// @file test_xfs_helpers.h
// @brief Shared image-construction and mount helpers for xFS host tests.
// All functions are static inline to suppress unused-function warnings when
// a test file does not exercise every helper.

#ifndef TEST_XFS_HELPERS_H
#define TEST_XFS_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "unity.h"

#include "xfs_block_ramdisk.h"
#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xfs_fat32_directory.h"

// TEST IMAGE CONSTANTS ///////////////////////////////////////////////////////

#define TEST_SECTOR_SIZE       512U
#define TEST_SECTOR_COUNT      64U
#define TEST_FAT_SECTOR        1U
#define TEST_ROOT_SECTOR       2U
#define TEST_CLUSTER_BASE      2U
#define TEST_ROOT_CLUSTER      2U
#define TEST_FILE_CLUSTER      3U
#define TEST_FILE_NEXT_CLUSTER 4U
#define TEST_DIR_CLUSTER       5U
#define TEST_NESTED_CLUSTER    6U
#define TEST_STORAGE_SIZE      (TEST_SECTOR_SIZE * TEST_SECTOR_COUNT)

// TEST IMAGE HELPERS /////////////////////////////////////////////////////////

static inline void test_write_fat_entry(uint8_t *storage, uint32_t cluster, uint32_t value)
{
    uint32_t offset = (TEST_FAT_SECTOR * TEST_SECTOR_SIZE) + (cluster * FAT32_ENTRY_SIZE);

    xWrite_LE32(&storage[offset], value);
}

static inline uint8_t *test_cluster_storage(uint8_t *storage, uint32_t cluster)
{
    return &storage[(size_t)(TEST_CLUSTER_BASE + (cluster - 2U)) * TEST_SECTOR_SIZE];
}

static inline void test_write_boot_sector(uint8_t *storage)
{
    uint8_t *boot = &storage[0U];

    boot[0U] = 0xEBU;
    boot[1U] = 0x58U;
    boot[2U] = 0x90U;

    xWrite_LE16(&boot[11U], TEST_SECTOR_SIZE);
    boot[13U] = 1U;
    xWrite_LE16(&boot[14U], 1U);
    boot[16U] = 1U;
    xWrite_LE32(&boot[32U], TEST_SECTOR_COUNT);
    xWrite_LE32(&boot[36U], 1U);
    xWrite_LE32(&boot[44U], TEST_ROOT_CLUSTER);
    xWrite_LE16(&boot[510U], 0xAA55U);
}

static inline void test_write_directory_entry_in_cluster(uint8_t *storage,
                                                         uint32_t dir_cluster,
                                                         uint32_t entry_index,
                                                         const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                                         uint8_t attributes,
                                                         uint32_t cluster,
                                                         uint32_t size)
{
    uint8_t *entry = test_cluster_storage(storage, dir_cluster) + ((size_t)entry_index * FAT32_DIRECTORY_ENTRY_SIZE);

    memcpy(entry, name, FAT32_DIRECTORY_NAME_LENGTH);
    entry[11U] = attributes;
    xWrite_LE16(&entry[20U], (uint16_t)((cluster >> 16U) & 0xFFFFU));
    xWrite_LE16(&entry[26U], (uint16_t)(cluster & 0xFFFFU));
    xWrite_LE32(&entry[28U], size);
}

static inline void test_write_directory_entry(uint8_t *storage,
                                              uint32_t entry_index,
                                              const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                              uint32_t cluster,
                                              uint32_t size)
{
    test_write_directory_entry_in_cluster(storage, TEST_ROOT_CLUSTER, entry_index, name, FAT32_ATTR_ARCHIVE, cluster, size);
}

static inline void test_mark_directory_entry_free(uint8_t *storage, uint32_t dir_cluster, uint32_t entry_index)
{
    uint8_t *entry = test_cluster_storage(storage, dir_cluster) + ((size_t)entry_index * FAT32_DIRECTORY_ENTRY_SIZE);

    entry[0U] = FAT32_DIRECTORY_ENTRY_FREE;
}

static inline void test_fill_directory_cluster_with_archives(uint8_t *storage, uint32_t dir_cluster)
{
    uint32_t entry_index;
    uint32_t name_index;

    for (entry_index = 0U; entry_index < (TEST_SECTOR_SIZE / FAT32_DIRECTORY_ENTRY_SIZE); entry_index++)
    {
        uint8_t *entry = test_cluster_storage(storage, dir_cluster) + ((size_t)entry_index * FAT32_DIRECTORY_ENTRY_SIZE);

        for (name_index = 0U; name_index < FAT32_DIRECTORY_NAME_LENGTH; name_index++)
        {
            entry[name_index] = (uint8_t)('A' + (entry_index % 26U));
        }

        entry[11U] = FAT32_ATTR_ARCHIVE;
    }
}

static inline void test_prepare_empty_image(uint8_t *storage)
{
    memset(storage, 0, (size_t)TEST_STORAGE_SIZE);
    test_write_boot_sector(storage);
    test_write_fat_entry(storage, TEST_ROOT_CLUSTER, FAT32_EOC_MIN);
}

static inline void test_fill_valid_fat_entries(uint8_t *storage, uint32_t value)
{
    uint32_t cluster;
    uint32_t cluster_limit = FAT32_CLUSTER_MIN + (TEST_SECTOR_COUNT - TEST_CLUSTER_BASE);

    for (cluster = FAT32_CLUSTER_MIN; cluster < cluster_limit; cluster++)
    {
        test_write_fat_entry(storage, cluster, value);
    }
}

static inline void test_prepare_file_image(uint8_t *storage)
{
    static const uint8_t hello_name[FAT32_DIRECTORY_NAME_LENGTH] = {'H', 'E', 'L', 'L', 'O', ' ', ' ', ' ', 'T', 'X', 'T'};
    static const uint8_t empty_name[FAT32_DIRECTORY_NAME_LENGTH] = {'E', 'M', 'P', 'T', 'Y', ' ', ' ', ' ', 'T', 'X', 'T'};
    uint32_t index;
    uint8_t *cluster_3 = test_cluster_storage(storage, TEST_FILE_CLUSTER);
    uint8_t *cluster_4 = test_cluster_storage(storage, TEST_FILE_NEXT_CLUSTER);

    test_prepare_empty_image(storage);
    test_write_fat_entry(storage, TEST_FILE_CLUSTER, TEST_FILE_NEXT_CLUSTER);
    test_write_fat_entry(storage, TEST_FILE_NEXT_CLUSTER, FAT32_EOC_MIN);
    test_write_directory_entry(storage, 0U, hello_name, TEST_FILE_CLUSTER, 600U);
    test_write_directory_entry(storage, 1U, empty_name, 0U, 0U);

    for (index = 0U; index < TEST_SECTOR_SIZE; index++)
    {
        cluster_3[index] = (uint8_t)(index & 0xFFU);
        cluster_4[index] = (uint8_t)(0x80U + (index & 0x7FU));
    }
}

static inline void test_prepare_nested_image(uint8_t *storage)
{
    static const uint8_t dir_name[FAT32_DIRECTORY_NAME_LENGTH] = {'S', 'U', 'B', 'D', 'I', 'R', ' ', ' ', ' ', ' ', ' '};
    static const uint8_t nested_name[FAT32_DIRECTORY_NAME_LENGTH] = {'N', 'E', 'S', 'T', ' ', ' ', ' ', ' ', 'T', 'X', 'T'};
    uint8_t *nested_cluster = test_cluster_storage(storage, TEST_NESTED_CLUSTER);

    test_prepare_empty_image(storage);
    test_write_fat_entry(storage, TEST_DIR_CLUSTER, FAT32_EOC_MIN);
    test_write_fat_entry(storage, TEST_NESTED_CLUSTER, FAT32_EOC_MIN);
    test_write_directory_entry_in_cluster(storage, TEST_ROOT_CLUSTER, 0U, dir_name, FAT32_ATTR_DIRECTORY, TEST_DIR_CLUSTER, 0U);
    test_write_directory_entry_in_cluster(storage, TEST_DIR_CLUSTER, 0U, nested_name, FAT32_ATTR_ARCHIVE, TEST_NESTED_CLUSTER, 4U);

    nested_cluster[0U] = 'N';
    nested_cluster[1U] = 'E';
    nested_cluster[2U] = 'S';
    nested_cluster[3U] = 'T';
}

static inline void test_mount_image(uint8_t *storage, xFS_RAMDisk_Context_t *ramdisk_ctx, xFS_Context_t *fs_ctx)
{
    ramdisk_ctx->storage = storage;
    ramdisk_ctx->sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx->sector_count = TEST_SECTOR_COUNT;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(fs_ctx, &gxFS_RAMDisk_Driver, ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(fs_ctx));
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, fs_ctx->bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(1U, fs_ctx->sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT32(TEST_FAT_SECTOR, fs_ctx->fat_start_sector);
    TEST_ASSERT_EQUAL_UINT32(TEST_CLUSTER_BASE, fs_ctx->cluster_heap_start);
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_CLUSTER, fs_ctx->root_dir_cluster);
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_COUNT - TEST_CLUSTER_BASE, fs_ctx->total_clusters);
}

#endif // TEST_XFS_HELPERS_H
// EOF /////////////////////////////////////////////////////////////////////////////
