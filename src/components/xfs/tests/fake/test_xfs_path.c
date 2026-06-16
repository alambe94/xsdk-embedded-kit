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

// @file test_xfs_path.c
// @brief Host tests for SFN conversion, path walking, and parent resolution.

#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "xfs_block_ramdisk.h"
#include "xfs_cache.h"
#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_directory.h"
#include "xfs_fat32_directory.h"
#include "xfs_file.h"
#include "xfs_path.h"

#include "test_xfs_helpers.h"

void setUp(void)
{
}
void tearDown(void)
{
}

// TESTS //////////////////////////////////////////////////////////////////////

void test_path_sfn_conversion_accepts_and_rejects_expected_names(void)
{
    uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH];
    static const uint8_t expected[FAT32_DIRECTORY_NAME_LENGTH] = {'H', 'E', 'L', 'L', 'O', ' ', ' ', ' ', 'T', 'X', 'T'};

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_To_SFN("hello.txt", 9U, sfn));
    TEST_ASSERT_EQUAL_MEMORY(expected, sfn, sizeof(expected));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Path_To_SFN("toolongname.txt", 15U, sfn));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Path_To_SFN(".hidden", 7U, sfn));
}

void test_path_walk_finds_root_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t entry_cluster;
    uint32_t entry_index;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Walk(&fs_ctx, &cache, "/hello.txt", &entry, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_CLUSTER, entry_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, entry_index);
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, entry.attributes);
    TEST_ASSERT_EQUAL_UINT32(600U, xRead_LE32((const uint8_t *)&entry.file_size));
}

void test_path_walk_finds_nested_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    uint8_t buffer[4U];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    xFS_File_t file;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint32_t bytes_read;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Walk(&fs_ctx, &cache, "/subdir/nest.txt", &entry, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL_UINT32(TEST_DIR_CLUSTER, entry_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, entry_index);
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, entry.attributes);
    TEST_ASSERT_EQUAL_UINT32(4U, xRead_LE32((const uint8_t *)&entry.file_size));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Open(&fs_ctx, &file, "/subdir/nest.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Read(&file, buffer, sizeof(buffer), &bytes_read));
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer), bytes_read);
    TEST_ASSERT_EQUAL_MEMORY("NEST", buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_File_Close(&file));
}

void test_path_parent_resolution_handles_nested_and_invalid_paths(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint32_t parent_cluster;
    uint8_t child_name[FAT32_DIRECTORY_NAME_LENGTH];
    static const uint8_t expected_nested_name[FAT32_DIRECTORY_NAME_LENGTH] = {'N', 'E', 'S', 'T', ' ', ' ', ' ', ' ', 'T', 'X', 'T'};
    static const uint8_t expected_new_name[FAT32_DIRECTORY_NAME_LENGTH] = {'N', 'E', 'W', ' ', ' ', ' ', ' ', ' ', 'T', 'X', 'T'};

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Resolve_Parent(&fs_ctx, &cache, "/subdir/nest.txt", &parent_cluster, child_name));
    TEST_ASSERT_EQUAL_UINT32(TEST_DIR_CLUSTER, parent_cluster);
    TEST_ASSERT_EQUAL_MEMORY(expected_nested_name, child_name, sizeof(child_name));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Resolve_Parent(&fs_ctx, &cache, "new.txt", &parent_cluster, child_name));
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_CLUSTER, parent_cluster);
    TEST_ASSERT_EQUAL_MEMORY(expected_new_name, child_name, sizeof(child_name));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_DIRECTORY,
                      xFS_Path_Resolve_Parent(&fs_ctx, &cache, "/subdir/nest.txt/child.txt", &parent_cluster, child_name));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_Path_Resolve_Parent(&fs_ctx, &cache, "/subdir/new.txt/", &parent_cluster, child_name));
}

void test_path_walk_accepts_repeated_separators(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Walk(&fs_ctx, &cache, "//subdir///nest.txt", &entry, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, entry.attributes);
    TEST_ASSERT_EQUAL_UINT32(4U, xRead_LE32((const uint8_t *)&entry.file_size));
}

void test_path_exists_returns_correct_results(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    bool exists;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Exists(&fs_ctx, "/hello.txt", &exists));
    TEST_ASSERT_TRUE(exists);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Exists(&fs_ctx, "/missing.txt", &exists));
    TEST_ASSERT_FALSE(exists);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Exists(&fs_ctx, "/empty.txt", &exists));
    TEST_ASSERT_TRUE(exists);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Exists(&fs_ctx, "/", &exists));
    TEST_ASSERT_TRUE(exists);
}

void test_path_stat_returns_correct_file_and_directory_metadata(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Stat_t stat;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/hello.txt", &stat));
    TEST_ASSERT_EQUAL_UINT32(600U, stat.size);
    TEST_ASSERT_EQUAL_UINT32(TEST_FILE_CLUSTER, stat.first_cluster);
    TEST_ASSERT_FALSE(stat.is_directory);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Path_Stat(&fs_ctx, "/missing.txt", &stat));

    test_prepare_nested_image(storage);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/subdir", &stat));
    TEST_ASSERT_TRUE(stat.is_directory);
    TEST_ASSERT_EQUAL_UINT32(TEST_DIR_CLUSTER, stat.first_cluster);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/", &stat));
    TEST_ASSERT_TRUE(stat.is_directory);
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_CLUSTER, stat.first_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, stat.size);
}

void test_path_rename_same_parent_renames_entry(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Stat_t stat;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Rename(&fs_ctx, "/hello.txt", "/world.txt"));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Path_Stat(&fs_ctx, "/hello.txt", &stat));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/world.txt", &stat));
    TEST_ASSERT_EQUAL_UINT32(600U, stat.size);
    TEST_ASSERT_FALSE(stat.is_directory);
}

void test_path_rename_rejects_existing_destination(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_ALREADY_EXISTS, xFS_Path_Rename(&fs_ctx, "/hello.txt", "/empty.txt"));
}

void test_path_rename_cross_directory_moves_file(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Stat_t stat;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Rename(&fs_ctx, "/subdir/nest.txt", "/moved.txt"));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND, xFS_Path_Stat(&fs_ctx, "/subdir/nest.txt", &stat));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/moved.txt", &stat));
    TEST_ASSERT_EQUAL_UINT32(4U, stat.size);
    TEST_ASSERT_FALSE(stat.is_directory);
}

void test_path_rename_rejects_null_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Path_Rename(NULL, "/hello.txt", "/world.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Path_Rename(&fs_ctx, NULL, "/world.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_Path_Rename(&fs_ctx, "/hello.txt", NULL));
}

void test_path_rename_rejects_unmounted_context(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_Path_Rename(&fs_ctx, "/hello.txt", "/world.txt"));
}

void test_path_rename_noop_when_source_equals_destination(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Stat_t stat;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Rename(&fs_ctx, "/hello.txt", "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Path_Stat(&fs_ctx, "/hello.txt", &stat));
    TEST_ASSERT_EQUAL_UINT32(600U, stat.size);
}

void test_path_rename_rejects_cross_directory_move_of_directory(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_nested_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Directory_Create(&fs_ctx, "/dir2"));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Path_Rename(&fs_ctx, "/subdir", "/dir2/subdir"));
}

// MAIN ///////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_path_sfn_conversion_accepts_and_rejects_expected_names);
    RUN_TEST(test_path_walk_finds_root_file);
    RUN_TEST(test_path_walk_finds_nested_file);
    RUN_TEST(test_path_parent_resolution_handles_nested_and_invalid_paths);
    RUN_TEST(test_path_walk_accepts_repeated_separators);
    RUN_TEST(test_path_exists_returns_correct_results);
    RUN_TEST(test_path_stat_returns_correct_file_and_directory_metadata);
    RUN_TEST(test_path_rename_same_parent_renames_entry);
    RUN_TEST(test_path_rename_rejects_existing_destination);
    RUN_TEST(test_path_rename_cross_directory_moves_file);
    RUN_TEST(test_path_rename_rejects_null_arguments);
    RUN_TEST(test_path_rename_rejects_unmounted_context);
    RUN_TEST(test_path_rename_noop_when_source_equals_destination);
    RUN_TEST(test_path_rename_rejects_cross_directory_move_of_directory);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
