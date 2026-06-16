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

// @file xfs_fat32_directory.c
// @brief FAT32 directory entry read, write, scan, and creation.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include <string.h>

#include "xfs_fat32_directory.h"
#include "xfs_fat32_cluster.h"
#include "xfs_fat32.h"
#include "xfs_defs.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static bool fat32_directory_name_matches(const xFS_FAT32_Directory_Entry_t *entry, const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH])
{
    uint32_t index;

    if ((entry == NULL) || (name == NULL))
    {
        return false;
    }

    for (index = 0U; index < FAT32_DIRECTORY_NAME_LENGTH; index++)
    {
        if (entry->name[index] != name[index])
        {
            return false;
        }
    }

    return true;
}

static xRETURN_t fat32_directory_zero_cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster)
{
    xRETURN_t status;
    uint32_t base_sector;
    uint32_t sector_index;

    if ((fs_ctx == NULL) || (cache == NULL) || !fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    base_sector = xFS_FAT32_Cluster_To_Sector(fs_ctx, cluster);
    (void)memset(cache->buffer, 0, sizeof(cache->buffer));

    for (sector_index = 0U; sector_index < fs_ctx->sectors_per_cluster; sector_index++)
    {
        cache->sector = base_sector + sector_index;
        cache->is_valid = true;
        cache->is_dirty = true;

        status = xFS_Cache_Write(fs_ctx, cache);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    return xRETURN_OK;
}

static xRETURN_t fat32_directory_extend_chain(xFS_Context_t *fs_ctx,
                                              xFS_Cache_Entry_t *cache,
                                              uint32_t tail_cluster,
                                              uint32_t *entry_cluster,
                                              uint32_t *entry_index)
{
    xRETURN_t status;
    uint32_t new_cluster;

    status = xFS_FAT32_Allocate_Cluster(fs_ctx, cache, &new_cluster);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = fat32_directory_zero_cluster(fs_ctx, cache, new_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, cache, new_cluster);
        return status;
    }

    status = xFS_FAT32_Write_Entry(fs_ctx, cache, tail_cluster, new_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, cache, new_cluster);
        return status;
    }

    status = xFS_Cache_Write(fs_ctx, cache);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, cache, new_cluster);
        return status;
    }

    *entry_cluster = new_cluster;
    *entry_index = 0U;

    return xRETURN_OK;
}

static xRETURN_t fat32_scan_cluster_for_entry(xFS_Context_t *fs_ctx,
                                              xFS_Cache_Entry_t *cache,
                                              uint32_t cluster,
                                              uint32_t entries_per_cluster,
                                              const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                              xFS_FAT32_Directory_Entry_t *entry_out,
                                              uint32_t *entry_cluster_out,
                                              uint32_t *entry_index_out,
                                              bool *found)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t current_entry;
    uint32_t index;

    *found = false;

    for (index = 0U; index < entries_per_cluster; index++)
    {
        status = xFS_FAT32_Directory_Read_Entry(fs_ctx, cache, cluster, index, &current_entry);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_FAT32_Directory_Is_End(&current_entry))
        {
            return xRETURN_xERR_xFS_NOT_FOUND;
        }

        if ((xFS_FAT32_Directory_Is_Free(&current_entry)) || (current_entry.attributes == FAT32_ATTR_LFN))
        {
            continue;
        }

        if (fat32_directory_name_matches(&current_entry, name))
        {
            *entry_out = current_entry;

            if (entry_cluster_out != NULL)
            {
                *entry_cluster_out = cluster;
            }

            if (entry_index_out != NULL)
            {
                *entry_index_out = index;
            }

            *found = true;
            return xRETURN_OK;
        }
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static xRETURN_t
fat32_directory_entry_location(xFS_Context_t *fs_ctx, uint32_t cluster, uint32_t entry_index, uint32_t *sector, uint32_t *offset)
{
    uint32_t entries_per_sector;
    uint32_t sector_index;
    uint32_t entry_index_in_sector;

    if ((fs_ctx == NULL) || (sector == NULL) || (offset == NULL) || !fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if ((fs_ctx->bytes_per_sector == 0U) || (fs_ctx->sectors_per_cluster == 0U))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    entries_per_sector = fs_ctx->bytes_per_sector / FAT32_DIRECTORY_ENTRY_SIZE;

    if (entries_per_sector == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    sector_index = entry_index / entries_per_sector;

    if (sector_index >= fs_ctx->sectors_per_cluster)
    {
        return xRETURN_xERR_xFS_OUT_OF_RANGE;
    }

    entry_index_in_sector = entry_index % entries_per_sector;

    *sector = xFS_FAT32_Cluster_To_Sector(fs_ctx, cluster) + sector_index;
    *offset = entry_index_in_sector * FAT32_DIRECTORY_ENTRY_SIZE;

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Directory_Read_Entry(xFS_Context_t *fs_ctx,
                                         xFS_Cache_Entry_t *cache,
                                         uint32_t cluster,
                                         uint32_t entry_index,
                                         xFS_FAT32_Directory_Entry_t *entry)
{
    xRETURN_t status;
    uint8_t *entry_ptr;
    uint32_t offset;
    uint32_t sector;

    if ((fs_ctx == NULL) || (cache == NULL) || (entry == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    status = fat32_directory_entry_location(fs_ctx, cluster, entry_index, &sector, &offset);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Cache_Read(fs_ctx, cache, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    entry_ptr = &cache->buffer[offset];

    (void)memcpy(entry, entry_ptr, sizeof(xFS_FAT32_Directory_Entry_t));

    return xRETURN_OK;
}

xRETURN_t xFS_FAT32_Directory_Write_Entry(xFS_Context_t *fs_ctx,
                                          xFS_Cache_Entry_t *cache,
                                          uint32_t cluster,
                                          uint32_t entry_index,
                                          const xFS_FAT32_Directory_Entry_t *entry)
{
    xRETURN_t status;
    uint8_t *entry_ptr;
    uint32_t offset;
    uint32_t sector;

    if ((fs_ctx == NULL) || (cache == NULL) || (entry == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    status = fat32_directory_entry_location(fs_ctx, cluster, entry_index, &sector, &offset);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Cache_Read(fs_ctx, cache, sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    entry_ptr = &cache->buffer[offset];

    (void)memcpy(entry_ptr, entry, sizeof(xFS_FAT32_Directory_Entry_t));

    cache->is_dirty = true;

    return xFS_Cache_Write(fs_ctx, cache);
}

bool xFS_FAT32_Directory_Is_End(const xFS_FAT32_Directory_Entry_t *entry)
{
    if (entry == NULL)
    {
        return false;
    }

    return (entry->name[0] == FAT32_DIRECTORY_ENTRY_END);
}

bool xFS_FAT32_Directory_Is_Free(const xFS_FAT32_Directory_Entry_t *entry)
{
    if (entry == NULL)
    {
        return false;
    }

    return (entry->name[0] == FAT32_DIRECTORY_ENTRY_FREE);
}

xRETURN_t xFS_FAT32_Directory_Find_Entry(xFS_Context_t *fs_ctx,
                                         xFS_Cache_Entry_t *cache,
                                         uint32_t start_cluster,
                                         const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                         xFS_FAT32_Directory_Entry_t *entry,
                                         uint32_t *entry_cluster,
                                         uint32_t *entry_index)
{
    xRETURN_t status;
    uint32_t current_cluster;
    uint32_t entries_per_cluster;
    uint32_t next_cluster;
    bool found;

    if ((fs_ctx == NULL) || (cache == NULL) || (name == NULL) || (entry == NULL) || !fat32_is_cluster_in_range(fs_ctx, start_cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    entries_per_cluster = fat32_entries_per_cluster(fs_ctx);

    if (entries_per_cluster == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    current_cluster = start_cluster;

    while (fat32_is_cluster_in_range(fs_ctx, current_cluster))
    {
        status = fat32_scan_cluster_for_entry(fs_ctx, cache, current_cluster, entries_per_cluster, name, entry, entry_cluster, entry_index,
                                              &found);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (found)
        {
            return xRETURN_OK;
        }

        status = xFS_FAT32_Read_Entry(fs_ctx, cache, current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return xRETURN_xERR_xFS_NOT_FOUND;
        }

        current_cluster = next_cluster;
    }

    xFS_LOG(xRETURN_xERR_xFS_CORRUPT, "cluster chain left valid range in find entry");
    return xRETURN_xERR_xFS_CORRUPT;
}

xRETURN_t xFS_FAT32_Directory_Find_Free_Entry(xFS_Context_t *fs_ctx,
                                              xFS_Cache_Entry_t *cache,
                                              uint32_t start_cluster,
                                              uint32_t *entry_cluster,
                                              uint32_t *entry_index)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t current_entry;
    uint32_t current_cluster;
    uint32_t current_index;
    uint32_t entries_per_cluster;
    uint32_t next_cluster;

    if ((fs_ctx == NULL) || (cache == NULL) || (entry_cluster == NULL) || (entry_index == NULL) ||
        !fat32_is_cluster_in_range(fs_ctx, start_cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    entries_per_cluster = fat32_entries_per_cluster(fs_ctx);

    if (entries_per_cluster == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    current_cluster = start_cluster;

    while (fat32_is_cluster_in_range(fs_ctx, current_cluster))
    {
        for (current_index = 0U; current_index < entries_per_cluster; current_index++)
        {
            status = xFS_FAT32_Directory_Read_Entry(fs_ctx, cache, current_cluster, current_index, &current_entry);

            if (status != xRETURN_OK)
            {
                return status;
            }

            if (xFS_FAT32_Directory_Is_End(&current_entry) || xFS_FAT32_Directory_Is_Free(&current_entry))
            {
                *entry_cluster = current_cluster;
                *entry_index = current_index;
                return xRETURN_OK;
            }
        }

        status = xFS_FAT32_Read_Entry(fs_ctx, cache, current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return fat32_directory_extend_chain(fs_ctx, cache, current_cluster, entry_cluster, entry_index);
        }

        current_cluster = next_cluster;
    }

    xFS_LOG(xRETURN_xERR_xFS_CORRUPT, "cluster chain left valid range in find free entry");
    return xRETURN_xERR_xFS_CORRUPT;
}

xRETURN_t xFS_FAT32_Directory_Create_File_Entry(xFS_Context_t *fs_ctx,
                                                xFS_Cache_Entry_t *cache,
                                                uint32_t parent_cluster,
                                                const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                                uint32_t *entry_cluster,
                                                uint32_t *entry_index)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t free_cluster;
    uint32_t free_index;

    if ((fs_ctx == NULL) || (cache == NULL) || (name == NULL) || (entry_cluster == NULL) || (entry_index == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    status = xFS_FAT32_Directory_Find_Entry(fs_ctx, cache, parent_cluster, name, &entry, NULL, NULL);

    if (status == xRETURN_OK)
    {
        return xRETURN_xERR_xFS_ALREADY_EXISTS;
    }

    status = xFS_FAT32_Directory_Find_Free_Entry(fs_ctx, cache, parent_cluster, &free_cluster, &free_index);

    if (status != xRETURN_OK)
    {
        return status;
    }

    (void)memset(&entry, 0, sizeof(entry));
    (void)memcpy(entry.name, name, FAT32_DIRECTORY_NAME_LENGTH);
    entry.attributes = FAT32_ATTR_ARCHIVE;

    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, cache, free_cluster, free_index, &entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    *entry_cluster = free_cluster;
    *entry_index = free_index;

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
