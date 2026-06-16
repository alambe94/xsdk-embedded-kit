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

// @file test_xfs_fat.c
// @brief Host tests for FAT32 chain allocation, cluster I/O, BPB validation,
//        and low-level directory entry access.

#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "xfs_block_ramdisk.h"
#include "xfs_cache.h"
#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_directory.h"
#include "xfs_fat32.h"
#include "xfs_fat32_bpb.h"
#include "xfs_fat32_cluster.h"
#include "xfs_fat32_directory.h"
#include "xfs_file.h"
#include "xfs_trace.h"

#include "test_xfs_helpers.h"

_Static_assert(sizeof(xFS_FAT32_Directory_Entry_t) == FAT32_DIRECTORY_ENTRY_SIZE, "FAT32 directory entry size must remain 32 bytes");

#define TEST_TRACE_BUF_BYTES 128U

static xTRACE_Time_t test_trace_timestamp(void *timestamp_ctx)
{
    uint32_t *tick = (uint32_t *)timestamp_ctx;
    xTRACE_Time_t timestamp = *tick;

    *tick = *tick + 1U;

    return timestamp;
}

void setUp(void)
{
}
void tearDown(void)
{
}

// TESTS //////////////////////////////////////////////////////////////////////

void test_fat_allocation_builds_and_releases_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint32_t first_cluster;
    uint32_t value;

    test_prepare_empty_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Allocate_Chain(&fs_ctx, &cache, 2U, &first_cluster));
    TEST_ASSERT_EQUAL_UINT32(3U, first_cluster);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 3U, &value));
    TEST_ASSERT_EQUAL_UINT32(4U, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 4U, &value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(value));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Release_Chain(&fs_ctx, &cache, first_cluster));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 3U, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 4U, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
}

void test_fat_entry_access_validates_arguments_and_masks_values(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint32_t value;
    uint32_t offset;

    test_prepare_empty_image(storage);
    test_write_fat_entry(storage, TEST_FILE_CLUSTER, 0xF0000000UL);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Read_Entry(NULL, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Read_Entry(&fs_ctx, NULL, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Read_Entry(&fs_ctx, &cache, 1U, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Write_Entry(NULL, &cache, TEST_FILE_CLUSTER, FAT32_EOC_MIN));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Write_Entry(&fs_ctx, NULL, TEST_FILE_CLUSTER, FAT32_EOC_MIN));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Write_Entry(&fs_ctx, &cache, 1U, FAT32_EOC_MIN));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(0U, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Write_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, 0x12345678UL));
    TEST_ASSERT_TRUE(cache.is_dirty);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(0x02345678UL, value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Write(&fs_ctx, &cache));

    offset = (TEST_FAT_SECTOR * TEST_SECTOR_SIZE) + (TEST_FILE_CLUSTER * FAT32_ENTRY_SIZE);
    TEST_ASSERT_EQUAL_UINT32(0xF2345678UL, xRead_LE32(&storage[offset]));
}

void test_fat_allocation_reports_disk_full_and_rolls_back_partial_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint32_t cluster;
    uint32_t first_cluster;
    uint32_t value;

    test_prepare_empty_image(storage);
    test_fill_valid_fat_entries(storage, FAT32_EOC_MIN);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Find_Free_Cluster(NULL, &cache, &cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Find_Free_Cluster(&fs_ctx, NULL, &cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Find_Free_Cluster(&fs_ctx, &cache, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Allocate_Cluster(NULL, &cache, &cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Allocate_Cluster(&fs_ctx, NULL, &cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Allocate_Cluster(&fs_ctx, &cache, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Allocate_Chain(NULL, &cache, 1U, &first_cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Allocate_Chain(&fs_ctx, NULL, 1U, &first_cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Allocate_Chain(&fs_ctx, &cache, 0U, &first_cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Allocate_Chain(&fs_ctx, &cache, 1U, NULL));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_DISK_FULL, xFS_FAT32_Find_Free_Cluster(&fs_ctx, &cache, &cluster));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_DISK_FULL, xFS_FAT32_Allocate_Cluster(&fs_ctx, &cache, &cluster));

    test_write_fat_entry(storage, TEST_FILE_CLUSTER, FAT32_FREE_CLUSTER);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Invalidate(&cache));
    first_cluster = 0xA5A5A5A5UL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_DISK_FULL, xFS_FAT32_Allocate_Chain(&fs_ctx, &cache, 2U, &first_cluster));
    TEST_ASSERT_EQUAL_UINT32(0U, first_cluster);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
}

void test_fat_release_rejects_invalid_and_reports_corrupt_chain(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint32_t value;
    uint32_t invalid_next_cluster = FAT32_CLUSTER_MIN + (TEST_SECTOR_COUNT - TEST_CLUSTER_BASE);

    test_prepare_empty_image(storage);
    test_write_fat_entry(storage, TEST_FILE_CLUSTER, invalid_next_cluster);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Release_Chain(NULL, &cache, TEST_FILE_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Release_Chain(&fs_ctx, NULL, TEST_FILE_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Release_Chain(&fs_ctx, &cache, 1U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_CORRUPT, xFS_FAT32_Release_Chain(&fs_ctx, &cache, TEST_FILE_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_FILE_CLUSTER, &value));
    TEST_ASSERT_EQUAL_UINT32(FAT32_FREE_CLUSTER, value);
}

void test_fat32_cluster_to_sector_maps_data_heap(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_empty_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL_UINT32(0U, xFS_FAT32_Cluster_To_Sector(NULL, TEST_ROOT_CLUSTER));
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_SECTOR, xFS_FAT32_Cluster_To_Sector(&fs_ctx, TEST_ROOT_CLUSTER));
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_SECTOR + 1U, xFS_FAT32_Cluster_To_Sector(&fs_ctx, TEST_FILE_CLUSTER));
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_SECTOR + 4U, xFS_FAT32_Cluster_To_Sector(&fs_ctx, TEST_NESTED_CLUSTER));
}

void test_fat32_cluster_read_and_write_use_cluster_sector(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Cluster_Read(&fs_ctx, &cache, TEST_FILE_CLUSTER));
    TEST_ASSERT_TRUE(cache.is_valid);
    TEST_ASSERT_FALSE(cache.is_dirty);
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_SECTOR + 1U, cache.sector);
    TEST_ASSERT_EQUAL_UINT8(0x00U, cache.buffer[0U]);
    TEST_ASSERT_EQUAL_UINT8(0xFFU, cache.buffer[255U]);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));
    cache.buffer[0U] = 0x11U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Cluster_Write(&fs_ctx, &cache, TEST_FILE_CLUSTER));
    TEST_ASSERT_TRUE(cache.is_valid);
    TEST_ASSERT_FALSE(cache.is_dirty);
    TEST_ASSERT_EQUAL_UINT32(TEST_ROOT_SECTOR + 1U, cache.sector);
    TEST_ASSERT_EQUAL_UINT8(0x11U, test_cluster_storage(storage, TEST_FILE_CLUSTER)[0U]);
    TEST_ASSERT_EQUAL_UINT8(0x00U, test_cluster_storage(storage, TEST_FILE_CLUSTER)[1U]);
}

void test_fat32_cluster_helpers_reject_null_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;

    test_prepare_empty_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Cluster_Read(NULL, &cache, TEST_ROOT_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Cluster_Read(&fs_ctx, NULL, TEST_ROOT_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Cluster_Write(NULL, &cache, TEST_ROOT_CLUSTER));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Cluster_Write(&fs_ctx, NULL, TEST_ROOT_CLUSTER));
}

void test_mount_rejects_invalid_boot_sector_geometry(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;

    test_prepare_empty_image(storage);
    xWrite_LE16(&storage[FAT32_BOOT_SIGNATURE_OFFSET], 0U);
    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = TEST_SECTOR_COUNT;
    memset(&fs_ctx, 0, sizeof(fs_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_STATE, xFS_Mount(&fs_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);

    test_prepare_empty_image(storage);
    xWrite_LE16(&storage[FAT32_BYTES_PER_SECTOR_OFFSET], 1024U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);

    test_prepare_empty_image(storage);
    storage[FAT32_SECTORS_PER_CLUSTER_OFFSET] = 0U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);

    test_prepare_empty_image(storage);
    xWrite_LE32(&storage[FAT32_ROOT_CLUSTER_OFFSET], 1U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);

    test_prepare_empty_image(storage);
    xWrite_LE32(&storage[FAT32_ROOT_CLUSTER_OFFSET], FAT32_CLUSTER_MIN + (TEST_SECTOR_COUNT - TEST_CLUSTER_BASE));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);

    test_prepare_empty_image(storage);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));
    xWrite_LE16(&storage[FAT32_BOOT_SIGNATURE_OFFSET], 0U);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_VOLUME, xFS_Mount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);
    TEST_ASSERT_EQUAL_UINT32(0U, fs_ctx.bytes_per_sector);
}

void test_unmount_blocks_subsequent_file_and_directory_open(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_File_t file;
    xFS_Directory_t directory;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));
    TEST_ASSERT_FALSE(fs_ctx.is_mounted);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_File_Open(&fs_ctx, &file, "/hello.txt"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_MOUNTED, xFS_Directory_Open(&fs_ctx, &directory, "/"));
}

void test_trace_init_attaches_context_and_mount_emits_event(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xTRACE_Context_t trace_ctx;
    uint8_t trace_buf[TEST_TRACE_BUF_BYTES];
    xTRACE_Config_t trace_cfg;
    uint32_t tick = 10U;

    test_prepare_empty_image(storage);
    ramdisk_ctx.storage = storage;
    ramdisk_ctx.sector_size = TEST_SECTOR_SIZE;
    ramdisk_ctx.sector_count = TEST_SECTOR_COUNT;

    (void)memset(&trace_ctx, 0, sizeof(trace_ctx));
    (void)memset(trace_buf, 0, sizeof(trace_buf));
    trace_cfg.buffer = trace_buf;
    trace_cfg.capacity_bytes = TEST_TRACE_BUF_BYTES;
    trace_cfg.timestamp_fn = test_trace_timestamp;
    trace_cfg.timestamp_ctx = &tick;
    trace_cfg.timestamp_hz = 1000000U;
    trace_cfg.is_enabled = true;

    TEST_ASSERT_EQUAL(xRETURN_OK, xTRACE_Init(&trace_ctx, &trace_cfg, NULL, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_Trace_Init(NULL, &trace_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Init(&fs_ctx, &gxFS_RAMDisk_Driver, &ramdisk_ctx));
    TEST_ASSERT_NULL(fs_ctx.trace_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Trace_Init(&fs_ctx, &trace_ctx));
    TEST_ASSERT_EQUAL_PTR(&trace_ctx, fs_ctx.trace_ctx);

    uint32_t pos_before_mount = trace_ctx.write_pos;
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Mount(&fs_ctx));

    // A MOUNT event must have been emitted (write_pos advanced).
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before_mount, trace_ctx.write_pos);
    // The byte after the length prefix of the MOUNT record is the event_id: 0x40 = xFS_TRACE_CODE_MOUNT.
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xFS_TRACE_CODE_MOUNT, trace_buf[pos_before_mount + 1U]);

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Trace_Init(&fs_ctx, NULL));
    TEST_ASSERT_NULL(fs_ctx.trace_ctx);

    uint32_t pos_before_unmount = trace_ctx.write_pos;
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Unmount(&fs_ctx));
    // No trace context attached - write_pos must not advance.
    TEST_ASSERT_EQUAL_UINT32(pos_before_unmount, trace_ctx.write_pos);
}

void test_fat32_directory_entry_access_validates_arguments_and_predicates(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    xFS_FAT32_Directory_Entry_t output_entry;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Read_Entry(NULL, &cache, TEST_ROOT_CLUSTER, 0U, &output_entry));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Read_Entry(&fs_ctx, NULL, TEST_ROOT_CLUSTER, 0U, &output_entry));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 0U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, 1U, 0U, &output_entry));
    TEST_ASSERT_EQUAL(
        xRETURN_xERR_xFS_OUT_OF_RANGE,
        xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, TEST_SECTOR_SIZE / FAT32_DIRECTORY_ENTRY_SIZE, &output_entry));

    memset(&entry, 0, sizeof(entry));
    memcpy(entry.name, "WRITTEN TXT", FAT32_DIRECTORY_NAME_LENGTH);
    entry.attributes = FAT32_ATTR_ARCHIVE;
    xWrite_LE32((uint8_t *)&entry.file_size, 17U);

    TEST_ASSERT_FALSE(xFS_FAT32_Directory_Is_End(NULL));
    TEST_ASSERT_FALSE(xFS_FAT32_Directory_Is_Free(NULL));
    memset(&output_entry, 0, sizeof(output_entry));
    TEST_ASSERT_TRUE(xFS_FAT32_Directory_Is_End(&output_entry));
    output_entry.name[0U] = FAT32_DIRECTORY_ENTRY_FREE;
    TEST_ASSERT_TRUE(xFS_FAT32_Directory_Is_Free(&output_entry));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Write_Entry(NULL, &cache, TEST_ROOT_CLUSTER, 2U, &entry));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Write_Entry(&fs_ctx, NULL, TEST_ROOT_CLUSTER, 2U, &entry));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER, xFS_FAT32_Directory_Write_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 2U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT, xFS_FAT32_Directory_Write_Entry(&fs_ctx, &cache, 1U, 2U, &entry));
    TEST_ASSERT_EQUAL(
        xRETURN_xERR_xFS_OUT_OF_RANGE,
        xFS_FAT32_Directory_Write_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, TEST_SECTOR_SIZE / FAT32_DIRECTORY_ENTRY_SIZE, &entry));

    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Write_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 2U, &entry));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, 2U, &output_entry));
    TEST_ASSERT_EQUAL_MEMORY(entry.name, output_entry.name, sizeof(entry.name));
    TEST_ASSERT_EQUAL_UINT8(FAT32_ATTR_ARCHIVE, output_entry.attributes);
    TEST_ASSERT_EQUAL_UINT32(17U, xRead_LE32((const uint8_t *)&output_entry.file_size));
}

void test_fat32_directory_find_entry_and_free_entry_edge_paths(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint8_t missing_name[FAT32_DIRECTORY_NAME_LENGTH] = {'M', 'I', 'S', 'S', 'I', 'N', 'G', ' ', 'T', 'X', 'T'};
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint32_t invalid_cluster = FAT32_CLUSTER_MIN + (TEST_SECTOR_COUNT - TEST_CLUSTER_BASE);
    uint32_t fat_value;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Entry(NULL, &cache, TEST_ROOT_CLUSTER, missing_name, &entry, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, NULL, TEST_ROOT_CLUSTER, missing_name, &entry, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, NULL, &entry, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, missing_name, NULL, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, &cache, 1U, missing_name, &entry, &entry_cluster, &entry_index));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Free_Entry(NULL, &cache, TEST_ROOT_CLUSTER, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, NULL, TEST_ROOT_CLUSTER, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, NULL, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, &entry_cluster, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_INVALID_ARGUMENT,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, 1U, &entry_cluster, &entry_index));

    test_fill_directory_cluster_with_archives(storage, TEST_ROOT_CLUSTER);
    test_write_fat_entry(storage, TEST_ROOT_CLUSTER, FAT32_EOC_MIN);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Invalidate(&cache));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NOT_FOUND,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, missing_name, &entry, NULL, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL_UINT32(TEST_DIR_CLUSTER, entry_cluster);
    TEST_ASSERT_EQUAL_UINT32(0U, entry_index);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, &fat_value));
    TEST_ASSERT_EQUAL_UINT32(TEST_DIR_CLUSTER, fat_value);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_FAT32_Read_Entry(&fs_ctx, &cache, TEST_DIR_CLUSTER, &fat_value));
    TEST_ASSERT_TRUE(xFS_IS_EOC(fat_value));

    test_prepare_file_image(storage);
    test_fill_valid_fat_entries(storage, FAT32_EOC_MIN);
    test_fill_directory_cluster_with_archives(storage, TEST_ROOT_CLUSTER);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Invalidate(&cache));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_DISK_FULL,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, &entry_cluster, &entry_index));

    test_write_fat_entry(storage, TEST_ROOT_CLUSTER, invalid_cluster);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Invalidate(&cache));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_CORRUPT,
                      xFS_FAT32_Directory_Find_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, missing_name, &entry, NULL, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_CORRUPT,
                      xFS_FAT32_Directory_Find_Free_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, &entry_cluster, &entry_index));
}

void test_fat32_directory_create_file_entry_validates_arguments(void)
{
    uint8_t storage[TEST_STORAGE_SIZE];
    xFS_RAMDisk_Context_t ramdisk_ctx;
    xFS_Context_t fs_ctx;
    xFS_Cache_Entry_t cache;
    uint8_t name[FAT32_DIRECTORY_NAME_LENGTH] = {'N', 'E', 'W', ' ', ' ', ' ', ' ', ' ', 'T', 'X', 'T'};
    uint32_t entry_cluster;
    uint32_t entry_index;

    test_prepare_file_image(storage);
    test_mount_image(storage, &ramdisk_ctx, &fs_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xFS_Cache_Init(&cache));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER,
                      xFS_FAT32_Directory_Create_File_Entry(NULL, &cache, TEST_ROOT_CLUSTER, name, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER,
                      xFS_FAT32_Directory_Create_File_Entry(&fs_ctx, NULL, TEST_ROOT_CLUSTER, name, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER,
                      xFS_FAT32_Directory_Create_File_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, NULL, &entry_cluster, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER,
                      xFS_FAT32_Directory_Create_File_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, name, NULL, &entry_index));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xFS_NULL_POINTER,
                      xFS_FAT32_Directory_Create_File_Entry(&fs_ctx, &cache, TEST_ROOT_CLUSTER, name, &entry_cluster, NULL));
}

// MAIN ///////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fat_allocation_builds_and_releases_chain);
    RUN_TEST(test_fat_entry_access_validates_arguments_and_masks_values);
    RUN_TEST(test_fat_allocation_reports_disk_full_and_rolls_back_partial_chain);
    RUN_TEST(test_fat_release_rejects_invalid_and_reports_corrupt_chain);
    RUN_TEST(test_fat32_cluster_to_sector_maps_data_heap);
    RUN_TEST(test_fat32_cluster_read_and_write_use_cluster_sector);
    RUN_TEST(test_fat32_cluster_helpers_reject_null_arguments);
    RUN_TEST(test_mount_rejects_invalid_boot_sector_geometry);
    RUN_TEST(test_unmount_blocks_subsequent_file_and_directory_open);
    RUN_TEST(test_trace_init_attaches_context_and_mount_emits_event);
    RUN_TEST(test_fat32_directory_entry_access_validates_arguments_and_predicates);
    RUN_TEST(test_fat32_directory_find_entry_and_free_entry_edge_paths);
    RUN_TEST(test_fat32_directory_create_file_entry_validates_arguments);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
