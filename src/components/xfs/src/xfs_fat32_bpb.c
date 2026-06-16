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

// @file xfs_fat32_bpb.c
// @brief FAT32 BIOS Parameter Block parsing and validation.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_fat32_bpb.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_FAT32_BPB_Parse(xFS_FAT32_BPB_t *bpb, const uint8_t *sector_buffer)
{
    if ((bpb == NULL) || (sector_buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    bpb->bytes_per_sector = xRead_LE16(&sector_buffer[FAT32_BYTES_PER_SECTOR_OFFSET]);

    bpb->sectors_per_cluster = sector_buffer[FAT32_SECTORS_PER_CLUSTER_OFFSET];

    bpb->reserved_sector_count = xRead_LE16(&sector_buffer[FAT32_RESERVED_SECTOR_COUNT_OFFSET]);

    bpb->number_of_fats = sector_buffer[FAT32_NUMBER_OF_FATS_OFFSET];

    bpb->total_sectors = xRead_LE32(&sector_buffer[FAT32_TOTAL_SECTORS_32_OFFSET]);

    bpb->fat_size = xRead_LE32(&sector_buffer[FAT32_SIZE_32_OFFSET]);

    bpb->root_dir_cluster = xRead_LE32(&sector_buffer[FAT32_ROOT_CLUSTER_OFFSET]);

    bpb->fat_start_sector = bpb->reserved_sector_count;

    bpb->cluster_heap_start = bpb->reserved_sector_count + ((uint32_t)bpb->number_of_fats * bpb->fat_size);

    if (bpb->sectors_per_cluster != 0U)
    {
        bpb->total_clusters = (bpb->total_sectors - bpb->cluster_heap_start) / bpb->sectors_per_cluster;
    }
    else
    {
        bpb->total_clusters = 0U;
    }

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_BPB_Validate(const xFS_FAT32_BPB_t *bpb, const uint8_t *sector_buffer)
{
    uint16_t signature;

    if ((bpb == NULL) || (sector_buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    signature = xRead_LE16(&sector_buffer[FAT32_BOOT_SIGNATURE_OFFSET]);

    if (signature != FAT32_BOOT_SIGNATURE_VALUE)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "bad boot signature");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->bytes_per_sector != XFS_SECTOR_SIZE)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "unsupported sector size");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->sectors_per_cluster == 0U)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "sectors per cluster is zero");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->number_of_fats == 0U)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "FAT count is zero");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->fat_size == 0U)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "FAT size is zero");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->total_sectors <= bpb->cluster_heap_start)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "total sectors <= cluster heap start");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (bpb->total_clusters == 0U)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "cluster count is zero");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if (!xFS_IS_VALID_CLUSTER(bpb->root_dir_cluster))
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "invalid root cluster");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    if ((bpb->root_dir_cluster - FAT32_CLUSTER_MIN) >= bpb->total_clusters)
    {
        xFS_LOG(xRETURN_xERR_xFS_INVALID_VOLUME, "root cluster outside data region");
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
