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

// @file xfs_fat32.c
// @brief FAT32 entry read/write, cluster allocation, chain release.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_fat32.h"
#include "xfs_defs.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_FAT32_Read_Entry(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster, uint32_t *value)
{
    xRETURN_t status;

    uint32_t fat_offset;
    uint32_t sector;
    uint32_t sector_offset;

    if ((fs_ctx == NULL) || (cache == NULL) || (value == NULL) || !fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    fat_offset = cluster * FAT32_ENTRY_SIZE;

    sector = fs_ctx->fat_start_sector + (fat_offset / fs_ctx->bytes_per_sector);

    sector_offset = fat_offset % fs_ctx->bytes_per_sector;

    status = xFS_Cache_Read(fs_ctx, cache, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    *value = xRead_LE32(&cache->buffer[sector_offset]) & FAT32_ENTRY_VALUE_MASK;

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Write_Entry(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster, uint32_t value)
{
    xRETURN_t status;

    uint32_t fat_offset;
    uint32_t sector;
    uint32_t sector_offset;

    if ((fs_ctx == NULL) || (cache == NULL) || !fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    fat_offset = cluster * FAT32_ENTRY_SIZE;

    sector = fs_ctx->fat_start_sector + (fat_offset / fs_ctx->bytes_per_sector);

    sector_offset = fat_offset % fs_ctx->bytes_per_sector;

    status = xFS_Cache_Read(fs_ctx, cache, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    value = (xRead_LE32(&cache->buffer[sector_offset]) & ~FAT32_ENTRY_VALUE_MASK) | (value & FAT32_ENTRY_VALUE_MASK);

    xWrite_LE32(&cache->buffer[sector_offset], value);

    cache->is_dirty = true;

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Find_Free_Cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t *cluster)
{
    xRETURN_t status;
    uint32_t current_cluster;
    uint32_t value;

    if ((fs_ctx == NULL) || (cache == NULL) || (cluster == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    for (current_cluster = FAT32_CLUSTER_MIN; current_cluster < (FAT32_CLUSTER_MIN + fs_ctx->total_clusters); current_cluster++)
    {
        status = xFS_FAT32_Read_Entry(fs_ctx, cache, current_cluster, &value);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (value == FAT32_FREE_CLUSTER)
        {
            *cluster = current_cluster;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xFS_DISK_FULL;
}

xRETURN_t xFS_FAT32_Allocate_Cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t *cluster)
{
    xRETURN_t status;
    uint32_t free_cluster;

    if ((fs_ctx == NULL) || (cache == NULL) || (cluster == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    status = xFS_FAT32_Find_Free_Cluster(fs_ctx, cache, &free_cluster);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_FAT32_Write_Entry(fs_ctx, cache, free_cluster, FAT32_EOC_MIN);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Cache_Write(fs_ctx, cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    *cluster = free_cluster;

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Allocate_Chain(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t length, uint32_t *first_cluster)
{
    xRETURN_t status;
    uint32_t index;
    uint32_t previous_cluster;
    uint32_t current_cluster;

    if ((fs_ctx == NULL) || (cache == NULL) || (first_cluster == NULL) || (length == 0U))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    *first_cluster = 0U;
    previous_cluster = 0U;

    for (index = 0U; index < length; index++)
    {
        status = xFS_FAT32_Allocate_Cluster(fs_ctx, cache, &current_cluster);

        if (status != xRETURN_OK)
        {
            if (*first_cluster != 0U)
            {
                (void)xFS_FAT32_Release_Chain(fs_ctx, cache, *first_cluster);
                *first_cluster = 0U;
            }

            return status;
        }

        if (*first_cluster == 0U)
        {
            *first_cluster = current_cluster;
        }

        if (previous_cluster != 0U)
        {
            status = xFS_FAT32_Write_Entry(fs_ctx, cache, previous_cluster, current_cluster);

            if (status != xRETURN_OK)
            {
                (void)xFS_FAT32_Release_Chain(fs_ctx, cache, *first_cluster);
                *first_cluster = 0U;
                return status;
            }

            status = xFS_Cache_Write(fs_ctx, cache);

            if (status != xRETURN_OK)
            {
                (void)xFS_FAT32_Release_Chain(fs_ctx, cache, *first_cluster);
                *first_cluster = 0U;
                return status;
            }
        }

        previous_cluster = current_cluster;
    }

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Release_Chain(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t first_cluster)
{
    xRETURN_t status;
    uint32_t current_cluster;
    uint32_t next_cluster;

    if ((fs_ctx == NULL) || (cache == NULL) || !fat32_is_cluster_in_range(fs_ctx, first_cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    current_cluster = first_cluster;

    while (fat32_is_cluster_in_range(fs_ctx, current_cluster))
    {
        status = xFS_FAT32_Read_Entry(fs_ctx, cache, current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        status = xFS_FAT32_Write_Entry(fs_ctx, cache, current_cluster, FAT32_FREE_CLUSTER);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return xFS_Cache_Write(fs_ctx, cache);
        }

        current_cluster = next_cluster;
    }

    xFS_LOG(xRETURN_xERR_xFS_CORRUPT, "cluster chain left valid range during release");
    return xRETURN_xERR_xFS_CORRUPT;
}
// EOF /////////////////////////////////////////////////////////////////////////////
