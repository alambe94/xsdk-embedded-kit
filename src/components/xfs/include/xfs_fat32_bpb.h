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

// @file xfs_fat32_bpb.h
// @brief FAT32 BIOS Parameter Block layout helpers.

#ifndef XFS_FAT32_BPB_H
#define XFS_FAT32_BPB_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define FAT32_BOOT_SIGNATURE_OFFSET 510U
#define FAT32_BOOT_SIGNATURE_VALUE  0xAA55U

#define FAT32_BYTES_PER_SECTOR_OFFSET      11U
#define FAT32_SECTORS_PER_CLUSTER_OFFSET   13U
#define FAT32_RESERVED_SECTOR_COUNT_OFFSET 14U
#define FAT32_NUMBER_OF_FATS_OFFSET        16U
#define FAT32_TOTAL_SECTORS_32_OFFSET      32U
#define FAT32_SIZE_32_OFFSET               36U
#define FAT32_ROOT_CLUSTER_OFFSET          44U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint16_t bytes_per_sector;

        uint8_t sectors_per_cluster;
        uint8_t number_of_fats;

        uint16_t reserved_sector_count;

        uint32_t total_sectors;
        uint32_t fat_size;

        uint32_t fat_start_sector;
        uint32_t cluster_heap_start;
        uint32_t total_clusters;

        uint32_t root_dir_cluster;

    } xFS_FAT32_BPB_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_FAT32_BPB_Parse(xFS_FAT32_BPB_t *bpb, const uint8_t *sector_buffer);

    xRETURN_t xFS_FAT32_BPB_Validate(const xFS_FAT32_BPB_t *bpb, const uint8_t *sector_buffer);

#ifdef __cplusplus
}
#endif

#endif // XFS_FAT32_BPB_H
// EOF /////////////////////////////////////////////////////////////////////////////
