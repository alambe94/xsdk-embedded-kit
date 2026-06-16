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

// @file test_xfs_file.c
// @brief Host tests for the public file and directory API, file create/delete,
//        truncate, directory iteration, and volume format.

#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "xfs_block_ramdisk.h"
#include "xfs_cache.h"
#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_directory.h"
#include "xfs_fat32.h"
#include "xfs_fat32_directory.h"
#include "xfs_file.h"
#include "xfs_format.h"
#include "xfs_path.h"

#include "test_xfs_helpers.h"

void setUp(void)
{
}
void tearDown(void)
{
}

// TESTS //////////////////////////////////////////////////////////////////////

void test_file_api_rejects_null_context(void)
{
    uint8_t buffer[4U] = {0U};
    uint32_t count = 0U;
    uint32_t position = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_File_Open(NULL, NULL, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Read(NULL, buffer, sizeof(buffer), &count));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Write(NULL, buffer, sizeof(buffer), &count));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Seek(NULL, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Tell(NULL, &position));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Flush(NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_File_Close(NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Truncate(NULL, 0U));
}

void test_file_api_rejects_null_output_pointers(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t buffer[4U] = {0U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t count = 0U;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Read(&file, NULL, sizeof(buffer), &count));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Read(&file, buffer, sizeof(buffer), NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Write(&file, NULL, sizeof(buffer), &count));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Write(&file, buffer, sizeof(buffer), NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Tell(&file, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_directory_api_rejects_null_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry = false;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Open(NULL, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Open(&fs_ctx, NULL, "/"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Open(&fs_ctx, &directory, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Directory_Read(NULL, &entry, &has_entry));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Directory_Read(&directory, NULL, &has_entry));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Directory_Read(&directory, &entry, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Close(NULL));
}

void test_file_read_handles_seek_and_cluster_boundary(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t buffer[600U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t bytes_read;
    uint32_t position;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), bytes_read);
    TEST_ASSERT_EQUAL_UINT8(0U, buffer[0U]);
    TEST_ASSERT_EQUAL_UINT8(255U, buffer[255U]);
    TEST_ASSERT_EQUAL_UINT8(0x80U, buffer[512U]);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(0x80U + 87U), buffer[599U]);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(600U, position);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 510U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, buffer, 4U, &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(4U, bytes_read);
    TEST_ASSERT_EQUAL_UINT8(254U, buffer[0U]);
    TEST_ASSERT_EQUAL_UINT8(255U, buffer[1U]);
    TEST_ASSERT_EQUAL_UINT8(0x80U, buffer[2U]);
    TEST_ASSERT_EQUAL_UINT8(0x81U, buffer[3U]);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_file_api_rejects_missing_path_and_invalid_seek(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t buffer[4U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t bytes_read;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_File_Open(&fs_ctx, &file, "/missing.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_OUT_OF_RANGE, xFS_File_Seek(&file, 601U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 600U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(0U, bytes_read);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_file_read_rejects_broken_fat_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t buffer[600U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t bytes_read;

    test_prepare_file_image(storage);
    test_write_fat_entry(storage, TEST_FILE_CLUSTER, FAT32_EOC_MIN);
    test_write_fat_entry(storage, TEST_FILE_NEXT_CLUSTER, FAT32_FREE_CLUSTER);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_CORRUPT, xFS_File_Read(&file, buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, bytes_read);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_file_write_overwrites_and_extends_existing_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t write_data[4U] = {0xA0U, 0xA1U, 0xA2U, 0xA3U};
    uint8_t read_data[4U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_done;
    uint32_t value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 598U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Flush(&file));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 598U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    TEST_ASSERT_EQUAL_UINT32(602U, xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_NEXT_CLUSTER, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
}

void test_file_write_grows_empty_file_and_updates_directory_entry(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t write_data[3U] = {'X', 'E', '!'};
    uint8_t read_data[3U] = {0U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_done;
    uint32_t value;
    uint32_t first_cluster;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/empty.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Flush(&file));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 1U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(5U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, first_cluster, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
}

void test_file_write_grows_empty_file_across_multiple_clusters(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t write_data[700U];
    uint8_t read_data[700U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_done;
    uint32_t first_cluster;
    uint32_t value;
    uint32_t index;

    test_prepare_file_image(storage);

    for (index = 0U; index < sizeof(write_data); index++)
    {
        write_data[index] = (uint8_t)(0x40U + (index & 0x3FU));
        read_data[index] = 0U;
    }

    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/empty.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Flush(&file));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 1U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(5U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 5U, &value));
    TEST_ASSERT_EQUAL_UINT32(6U, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 6U, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
}

void test_file_truncate_shrinks_file_and_releases_tail_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t read_data[401U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_read;
    uint32_t position;
    uint32_t value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 599U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Truncate(&file, 400U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(400U, position);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(400U, bytes_read);
    TEST_ASSERT_EQUAL_UINT8(0U, read_data[0U]);
    TEST_ASSERT_EQUAL_UINT8(143U, read_data[399U]);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    TEST_ASSERT_EQUAL_UINT32(400U, xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_NEXT_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
}

void test_file_truncate_to_current_size_is_noop(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t position;
    uint32_t value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 123U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Truncate(&file, 600U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(123U, position);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    TEST_ASSERT_EQUAL_UINT32(600U, xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(TEST_FILE_NEXT_CLUSTER, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_NEXT_CLUSTER, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
}

void test_file_truncate_to_zero_releases_all_clusters(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t read_data[1U] = {0xA5U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_read;
    uint32_t first_cluster;
    uint32_t value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Truncate(&file, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(0U, bytes_read);
    TEST_ASSERT_EQUAL_UINT8(0xA5U, read_data[0U]);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(0U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_NEXT_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
}

void test_file_truncate_grows_file_with_zero_fill(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t read_data[520U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_read;
    uint32_t position;
    uint32_t first_cluster;
    uint32_t value;
    uint32_t index;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/empty.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Truncate(&file, 520U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(0U, position);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_read);

    for (index = 0U; index < sizeof(read_data); index++)
    {
        TEST_ASSERT_EQUAL_UINT8(0U, read_data[index]);
    }

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 1U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(520U, xRead_LE32((const uint8_t *)&entry.file_size));
    TEST_ASSERT_EQUAL_UINT32(5U, first_cluster);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 5U, &value));
    TEST_ASSERT_EQUAL_UINT32(6U, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 6U, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));
}

void test_file_create_writes_new_root_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t write_data[5U] = {'C', 'R', 'E', 'A', '8'};
    uint8_t read_data[5U] = {0U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_done;
    uint32_t first_cluster;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/new.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/new.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 2U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(5U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), xRead_LE32((const uint8_t *)&entry.file_size));
}

void test_file_create_writes_new_nested_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t write_data[6U] = {'N', 'E', 'W', 'D', 'I', 'R'};
    uint8_t read_data[6U] = {0U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_done;
    uint32_t first_cluster;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/subdir/new.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/subdir/new.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_DIR_CLUSTER, 1U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(3U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), xRead_LE32((const uint8_t *)&entry.file_size));
}

void test_file_create_rejects_duplicate_and_trailing_separator(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_ALREADY_EXISTS, xFS_File_Create(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_File_Create(&fs_ctx, &file, "/brandnew/"));
}

void test_file_create_zero_byte_file_persists_empty_entry(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t read_data[1U] = {0xA5U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t bytes_read;
    uint32_t first_cluster;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/zero.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/zero.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_data, sizeof(read_data), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(0U, bytes_read);
    TEST_ASSERT_EQUAL_UINT8(0xA5U, read_data[0U]);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 2U, &entry));
    first_cluster = (((uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_high)) << 16) |
                    (uint32_t)xRead_LE16((const uint8_t *)&entry.first_cluster_low);
    TEST_ASSERT_EQUAL_UINT32(0U, first_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, xRead_LE32((const uint8_t *)&entry.file_size));
}

void test_file_create_reuses_free_directory_slot(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    static const uint8_t expected_name[FAT32_DIRECTORY_NAME_LENGTH] = {'R', 'E', 'U', 'S', 'E', ' ', ' ', ' ', 'T', 'X', 'T'};

    test_prepare_file_image(storage);
    test_mark_directory_entry_free(storage, TEST_ROOT_CLUSTER, 0U);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/reuse.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    TEST_ASSERT_EQUAL_MEMORY(expected_name, entry.name, sizeof(expected_name));
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, entry.attributes);
}

void test_file_delete_removes_root_file_and_releases_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Delete(&fs_ctx, "/hello.txt"));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, &entry));
    TEST_ASSERT_EQUAL_UINT8(FAT32_DIRECTORY_ENTRY_FREE, entry.name[0U]);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_NEXT_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
}

void test_file_delete_rejects_missing_path(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_File_Delete(&fs_ctx, "/missing.txt"));
}

void test_file_delete_rejects_directory_path(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FILE, xFS_File_Delete(&fs_ctx, "/subdir"));
}

void test_directory_read_iterates_root_entries(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING("HELLO.TXT", entry.name);
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, entry.attributes);
    TEST_ASSERT_FALSE(entry.is_directory);
    TEST_ASSERT_EQUAL_UINT32(TEST_FILE_CLUSTER, entry.first_cluster);
    TEST_ASSERT_EQUAL_UINT32(600U, entry.size);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING("EMPTY.TXT", entry.name);
    TEST_ASSERT_EQUAL_UINT32(0U, entry.first_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, entry.size);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_FALSE(has_entry);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_directory_read_iterates_nested_directory(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/subdir"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING("NEST.TXT", entry.name);
    TEST_ASSERT_FALSE(entry.is_directory);
    TEST_ASSERT_EQUAL_UINT32(TEST_NESTED_CLUSTER, entry.first_cluster);
    TEST_ASSERT_EQUAL_UINT32(4U, entry.size);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_FALSE(has_entry);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_directory_read_skips_free_entries_and_rejects_files(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    test_prepare_file_image(storage);
    test_mark_directory_entry_free(storage, TEST_ROOT_CLUSTER, 0U);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Directory_Open(&fs_ctx, &directory, ""));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_DIRECTORY, xFS_Directory_Open(&fs_ctx, &directory, "/empty.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "///"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING("EMPTY.TXT", entry.name);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_directory_read_rejects_broken_directory_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    test_prepare_nested_image(storage);
    test_write_fat_entry(storage, TEST_DIR_CLUSTER, TEST_SECTOR_COUNT);
    memset(test_cluster_storage(storage, TEST_DIR_CLUSTER), 0xE5, TEST_SECTOR_SIZE);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/subdir"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_CORRUPT, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_format_fat32_creates_mountable_empty_volume(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Format_Config_t format_config;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    memset(storage, 0xA5, (size_t)TEST_STORAGE_SIZE);
    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = TEST_SECTOR_COUNT;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_Config_Default(&format_config, TEST_SECTOR_COUNT));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, &format_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, fs_ctx.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(1U, fs_ctx.sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT32(TEST_FAT_SECTOR, fs_ctx.fat_start_sector);
    TEST_ASSERT_EQUAL_UINT32(TEST_CLUSTER_BASE, fs_ctx.cluster_heap_start);
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_CLUSTER, fs_ctx.root_dir_cluster);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_FALSE(has_entry);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_format_fat32_rejects_invalid_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Format_Config_t format_config;

    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = TEST_SECTOR_COUNT;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Format_Config_Default(NULL, TEST_SECTOR_COUNT));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_Config_Default(&format_config, TEST_SECTOR_COUNT));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Format_FAT32(NULL, &ramdisk_ctx, &format_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, NULL));

    format_config.bytes_per_sector = 1024U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, &format_config));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_Config_Default(&format_config, TEST_SECTOR_COUNT));
    format_config.fat_size = 0x01U;
    format_config.sector_count = 1024U * 1024U;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, &format_config));
}

void test_file_tell_tracks_write_position(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t data[4U] = {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t count;
    uint32_t position;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(0U, position);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, data, sizeof(data), &count));
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), count);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(4U, position);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 596U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, data, sizeof(data), &count));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Tell(&file, &position));
    TEST_ASSERT_EQUAL_UINT32(600U, position);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_file_seek_refreshes_stale_reader_cache(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t read_data[4U];
    uint8_t write_data[4U] = {'N', 'E', 'W', '!'};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t reader;
    xFS_File_t writer;
    uint32_t bytes_done;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &reader, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&reader, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &writer, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&writer, write_data, sizeof(write_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_data), bytes_done);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Flush(&writer));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&reader, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&reader, read_data, sizeof(read_data), &bytes_done));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_data), bytes_done);
    TEST_ASSERT_EQUAL_MEMORY(write_data, read_data, sizeof(write_data));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&writer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&reader));
}

void test_directory_open_rejects_missing_path(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Directory_Open(&fs_ctx, &directory, "/missing"));

    test_prepare_nested_image(storage);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Directory_Open(&fs_ctx, &directory, "/subdir/missing"));
}

void test_file_write_fails_on_full_disk(void)
{
    static const uint8_t empty_name[FAT32_DIRECTORY_NAME_LENGTH] = {'E', 'M', 'P', 'T', 'Y', ' ', ' ', ' ', 'T', 'X', 'T'};
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t data[4U] = {1U, 2U, 3U, 4U};
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    uint32_t count;

    test_prepare_empty_image(storage);
    test_fill_valid_fat_entries(storage, FAT32_EOC_MIN);
    test_write_directory_entry(storage, 0U, empty_name, 0U, 0U);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/empty.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_DISK_FULL, xFS_File_Write(&file, data, sizeof(data), &count));
    TEST_ASSERT_EQUAL_UINT32(0U, count);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_format_fat32_large_media_auto_fat_size(void)
{
    static uint8_t storage[256U * TEST_SECTOR_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Format_Config_t config;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;

    memset(storage, 0xA5U, sizeof(storage));
    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = 256U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_Config_Default(&config, 256U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, &config));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));

    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, fs_ctx.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(1U, fs_ctx.sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT32(1U, fs_ctx.fat_start_sector);
    TEST_ASSERT_EQUAL_UINT32(3U, fs_ctx.cluster_heap_start);
    TEST_ASSERT_EQUAL_UINT32(FAT32_CLUSTER_MIN, fs_ctx.root_dir_cluster);
    TEST_ASSERT_EQUAL_UINT32(253U, fs_ctx.total_clusters);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_FALSE(has_entry);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_format_fat32_multi_sector_cluster_read_write(void)
{
    static uint8_t storage[128U * TEST_SECTOR_SIZE];
    static uint8_t write_buf[600U];
    uint8_t read_buf[600U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Format_Config_t config;
    xFS_File_t file;
    uint32_t bytes_written;
    uint32_t bytes_read;
    uint32_t i;

    for (i = 0U; i < sizeof(write_buf); i++)
    {
        write_buf[i] = (uint8_t)(i & 0xFFU);
    }

    memset(storage, 0xA5U, sizeof(storage));
    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = 128U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_Config_Default(&config, 128U));
    config.sectors_per_cluster = 4U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Format_FAT32(&gxFS_RAMDisk_Driver, &ramdisk_ctx, &config));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));

    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, fs_ctx.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(4U, fs_ctx.sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT32(1U, fs_ctx.fat_start_sector);
    TEST_ASSERT_EQUAL_UINT32(2U, fs_ctx.cluster_heap_start);
    TEST_ASSERT_EQUAL_UINT32(FAT32_CLUSTER_MIN, fs_ctx.root_dir_cluster);
    TEST_ASSERT_EQUAL_UINT32(31U, fs_ctx.total_clusters);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/DATA.BIN"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Write(&file, write_buf, sizeof(write_buf), &bytes_written));
    TEST_ASSERT_EQUAL_UINT32(sizeof(write_buf), bytes_written);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/DATA.BIN"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Seek(&file, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, read_buf, sizeof(read_buf), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(sizeof(read_buf), bytes_read);
    TEST_ASSERT_EQUAL_MEMORY(write_buf, read_buf, sizeof(read_buf));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_directory_rewind_restarts_iteration(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    bool has_entry;
    char first_name[XFS_DIRECTORY_NAME_MAX];

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    memcpy(first_name, entry.name, sizeof(first_name));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Rewind(&directory));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING(first_name, entry.name);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_directory_rewind_refreshes_stale_iterator_cache(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t directory;
    xFS_Directory_Entry_t entry;
    xFS_File_t file;
    bool has_entry;
    bool found_new_file;
    uint32_t guard;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &directory, "/"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Create(&fs_ctx, &file, "/third.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Rewind(&directory));

    found_new_file = false;
    for (guard = 0U; guard < 8U; guard++)
    {
        TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&directory, &entry, &has_entry));

        if (has_entry == false)
        {
            break;
        }

        if (strcmp(entry.name, "THIRD.TXT") == 0)
        {
            found_new_file = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(found_new_file);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&directory));
}

void test_volume_get_info_returns_correct_values(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Volume_Info_t info;
    static const uint8_t label_name[FAT32_DIRECTORY_NAME_LENGTH] = {'M', 'Y', 'D', 'I', 'S', 'K', ' ', ' ', ' ', ' ', ' '};

    test_prepare_empty_image(storage);
    test_write_directory_entry_in_cluster(storage, TEST_ROOT_CLUSTER, 0U, label_name, FAT32_ATTR_VOLUME_ID, 0U, 0U);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Volume_Get_Info(&fs_ctx, &info));

    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_COUNT - TEST_CLUSTER_BASE, info.total_clusters);
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, info.bytes_per_sector);
    TEST_ASSERT_EQUAL_UINT32(1U, info.sectors_per_cluster);
    TEST_ASSERT_EQUAL_UINT32(TEST_SECTOR_SIZE, info.bytes_per_cluster);
    TEST_ASSERT_EQUAL_UINT32((TEST_SECTOR_COUNT - TEST_CLUSTER_BASE) - 1U, info.free_clusters);
    TEST_ASSERT_EQUAL_STRING("MYDISK", info.label);
}

void test_sync_returns_ok_on_mounted_volume(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Sync(&fs_ctx));
}

void test_sync_rejects_unmounted_context(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_Sync(&fs_ctx));
}

void test_directory_create_creates_empty_scannable_directory(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Directory_t dir;
    xFS_Directory_Entry_t entry;
    xFS_Stat_t stat;
    bool has_entry;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Create(&fs_ctx, "/newdir"));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/newdir", &stat));
    TEST_ASSERT_TRUE(stat.is_directory);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Open(&fs_ctx, &dir, "/newdir"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&dir, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING(".", entry.name);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&dir, &entry, &has_entry));
    TEST_ASSERT_TRUE(has_entry);
    TEST_ASSERT_EQUAL_STRING("..", entry.name);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Read(&dir, &entry, &has_entry));
    TEST_ASSERT_FALSE(has_entry);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Close(&dir));
}

void test_directory_create_rejects_existing(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_ALREADY_EXISTS, xFS_Directory_Create(&fs_ctx, "/subdir"));
}

void test_directory_create_rejects_null_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Create(NULL, "/newdir"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Create(&fs_ctx, NULL));
}

void test_directory_create_rejects_unmounted_context(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_Directory_Create(&fs_ctx, "/newdir"));
}

void test_directory_delete_rejects_null_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Delete(NULL, "/newdir"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Directory_Delete(&fs_ctx, NULL));
}

void test_directory_delete_rejects_unmounted_context(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_Directory_Delete(&fs_ctx, "/subdir"));
}

void test_directory_delete_removes_empty_directory(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Stat_t stat;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Create(&fs_ctx, "/newdir"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Delete(&fs_ctx, "/newdir"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Path_Stat(&fs_ctx, "/newdir", &stat));
}

void test_directory_delete_rejects_nonempty_directory(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_EMPTY, xFS_Directory_Delete(&fs_ctx, "/subdir"));
}

void test_directory_delete_rejects_file_path(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_DIRECTORY, xFS_Directory_Delete(&fs_ctx, "/hello.txt"));
}

// MAIN ///////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_file_api_rejects_null_context);
    RUN_TEST(test_file_api_rejects_null_output_pointers);
    RUN_TEST(test_directory_api_rejects_null_arguments);
    RUN_TEST(test_file_read_handles_seek_and_cluster_boundary);
    RUN_TEST(test_file_api_rejects_missing_path_and_invalid_seek);
    RUN_TEST(test_file_read_rejects_broken_fat_chain);
    RUN_TEST(test_file_write_overwrites_and_extends_existing_file);
    RUN_TEST(test_file_write_grows_empty_file_and_updates_directory_entry);
    RUN_TEST(test_file_write_grows_empty_file_across_multiple_clusters);
    RUN_TEST(test_file_truncate_shrinks_file_and_releases_tail_chain);
    RUN_TEST(test_file_truncate_to_current_size_is_noop);
    RUN_TEST(test_file_truncate_to_zero_releases_all_clusters);
    RUN_TEST(test_file_truncate_grows_file_with_zero_fill);
    RUN_TEST(test_file_create_writes_new_root_file);
    RUN_TEST(test_file_create_writes_new_nested_file);
    RUN_TEST(test_file_create_rejects_duplicate_and_trailing_separator);
    RUN_TEST(test_file_create_zero_byte_file_persists_empty_entry);
    RUN_TEST(test_file_create_reuses_free_directory_slot);
    RUN_TEST(test_file_delete_removes_root_file_and_releases_chain);
    RUN_TEST(test_file_delete_rejects_missing_path);
    RUN_TEST(test_file_delete_rejects_directory_path);
    RUN_TEST(test_directory_read_iterates_root_entries);
    RUN_TEST(test_directory_read_iterates_nested_directory);
    RUN_TEST(test_directory_read_skips_free_entries_and_rejects_files);
    RUN_TEST(test_directory_read_rejects_broken_directory_chain);
    RUN_TEST(test_format_fat32_creates_mountable_empty_volume);
    RUN_TEST(test_format_fat32_rejects_invalid_arguments);
    RUN_TEST(test_format_fat32_large_media_auto_fat_size);
    RUN_TEST(test_format_fat32_multi_sector_cluster_read_write);
    RUN_TEST(test_file_tell_tracks_write_position);
    RUN_TEST(test_file_seek_refreshes_stale_reader_cache);
    RUN_TEST(test_directory_open_rejects_missing_path);
    RUN_TEST(test_file_write_fails_on_full_disk);
    RUN_TEST(test_directory_rewind_restarts_iteration);
    RUN_TEST(test_directory_rewind_refreshes_stale_iterator_cache);
    RUN_TEST(test_volume_get_info_returns_correct_values);
    RUN_TEST(test_sync_returns_ok_on_mounted_volume);
    RUN_TEST(test_sync_rejects_unmounted_context);
    RUN_TEST(test_directory_create_creates_empty_scannable_directory);
    RUN_TEST(test_directory_create_rejects_existing);
    RUN_TEST(test_directory_create_rejects_null_arguments);
    RUN_TEST(test_directory_create_rejects_unmounted_context);
    RUN_TEST(test_directory_delete_removes_empty_directory);
    RUN_TEST(test_directory_delete_rejects_nonempty_directory);
    RUN_TEST(test_directory_delete_rejects_file_path);
    RUN_TEST(test_directory_delete_rejects_null_arguments);
    RUN_TEST(test_directory_delete_rejects_unmounted_context);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
