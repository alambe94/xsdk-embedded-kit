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

// @file xfs_directory.c
// @brief xFS directory iteration API.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include <string.h>

#include "xfs_directory.h"
#include "xfs_cache.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xfs_fat32_cluster.h"
#include "xfs_path.h"
#include "xfs_trace.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static bool directory_path_is_root(const char *path)
{
    const char *cursor;

    if (path == NULL)
    {
        return false;
    }

    if (*path == '\0')
    {
        return false;
    }

    cursor = path;

    while (path_is_separator(*cursor))
    {
        cursor++;
    }

    return (*cursor == '\0');
}

static void directory_sfn_to_name(const uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH], char name[XFS_DIRECTORY_NAME_MAX])
{
    uint32_t index;
    uint32_t output_index;
    uint32_t name_end;
    uint32_t extension_end;

    output_index = 0U;
    name_end = 8U;
    extension_end = FAT32_DIRECTORY_NAME_LENGTH;

    while ((name_end > 0U) && (sfn[name_end - 1U] == (uint8_t)' '))
    {
        name_end--;
    }

    while ((extension_end > 8U) && (sfn[extension_end - 1U] == (uint8_t)' '))
    {
        extension_end--;
    }

    for (index = 0U; index < name_end; index++)
    {
        name[output_index] = (char)sfn[index];
        output_index++;
    }

    if (extension_end > 8U)
    {
        name[output_index] = '.';
        output_index++;

        for (index = 8U; index < extension_end; index++)
        {
            name[output_index] = (char)sfn[index];
            output_index++;
        }
    }

    name[output_index] = '\0';
}

static void directory_public_entry_from_fat32(const xFS_FAT32_Directory_Entry_t *fat_entry, xFS_Directory_Entry_t *entry)
{
    if ((fat_entry == NULL) || (entry == NULL))
    {
        return;
    }

    directory_sfn_to_name(fat_entry->name, entry->name);
    entry->attributes = fat_entry->attributes;
    entry->first_cluster = fat32_entry_first_cluster(fat_entry);
    entry->size = xRead_LE32((const uint8_t *)&fat_entry->file_size);
    entry->is_directory = ((fat_entry->attributes & FAT32_ATTR_DIRECTORY) != 0U);
}

static xRETURN_t directory_zero_new_cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster)
{
    xRETURN_t status;
    uint32_t base_sector;
    uint32_t sector_index;

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

static xRETURN_t directory_write_dot_entries(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t new_cluster, uint32_t parent_cluster)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t entry;
    static const uint8_t dot_name[FAT32_DIRECTORY_NAME_LENGTH] = {'.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    static const uint8_t dotdot_name[FAT32_DIRECTORY_NAME_LENGTH] = {'.', '.', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};

    (void)memset(&entry, 0, sizeof(entry));
    (void)memcpy(entry.name, dot_name, FAT32_DIRECTORY_NAME_LENGTH);
    entry.attributes = FAT32_ATTR_DIRECTORY;
    xWrite_LE16((uint8_t *)&entry.first_cluster_high, (uint16_t)((new_cluster >> 16U) & 0xFFFFU));
    xWrite_LE16((uint8_t *)&entry.first_cluster_low, (uint16_t)(new_cluster & 0xFFFFU));

    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, cache, new_cluster, 0U, &entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    (void)memcpy(entry.name, dotdot_name, FAT32_DIRECTORY_NAME_LENGTH);
    xWrite_LE16((uint8_t *)&entry.first_cluster_high, (uint16_t)((parent_cluster >> 16U) & 0xFFFFU));
    xWrite_LE16((uint8_t *)&entry.first_cluster_low, (uint16_t)(parent_cluster & 0xFFFFU));

    return xFS_FAT32_Directory_Write_Entry(fs_ctx, cache, new_cluster, 1U, &entry);
}

static bool directory_entry_is_dot(const xFS_FAT32_Directory_Entry_t *entry)
{
    return (entry->name[0U] == (uint8_t)'.');
}

static xRETURN_t directory_is_empty(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t start_cluster, bool *empty)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t current_cluster;
    uint32_t index;
    uint32_t entries_per_cluster;
    uint32_t next_cluster;

    entries_per_cluster = fat32_entries_per_cluster(fs_ctx);

    if (entries_per_cluster == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    *empty = true;
    current_cluster = start_cluster;

    while (fat32_is_cluster_in_range(fs_ctx, current_cluster))
    {
        for (index = 0U; index < entries_per_cluster; index++)
        {
            status = xFS_FAT32_Directory_Read_Entry(fs_ctx, cache, current_cluster, index, &entry);

            if (status != xRETURN_OK)
            {
                return status;
            }

            if (xFS_FAT32_Directory_Is_End(&entry))
            {
                return xRETURN_OK;
            }

            if (xFS_FAT32_Directory_Is_Free(&entry) || (entry.attributes == FAT32_ATTR_LFN) || directory_entry_is_dot(&entry))
            {
                continue;
            }

            *empty = false;
            return xRETURN_OK;
        }

        status = xFS_FAT32_Read_Entry(fs_ctx, cache, current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return xRETURN_OK;
        }

        current_cluster = next_cluster;
    }

    return xRETURN_xERR_xFS_CORRUPT;
}

static xRETURN_t directory_advance_cluster(xFS_Directory_t *directory)
{
    xRETURN_t status;
    uint32_t next_cluster;

    status = xFS_FAT32_Read_Entry(directory->fs_ctx, &directory->cache, directory->current_cluster, &next_cluster);

    if (status != xRETURN_OK)
    {
        return status;
    }

    if (xFS_IS_EOC(next_cluster))
    {
        directory->is_end = true;
        return xRETURN_OK;
    }

    if (!fat32_is_cluster_in_range(directory->fs_ctx, next_cluster))
    {
        return xRETURN_xERR_xFS_CORRUPT;
    }

    directory->current_cluster = next_cluster;
    directory->current_index = 0U;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_Directory_Create(xFS_Context_t *fs_ctx, const char *path)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    xFS_FAT32_Directory_Entry_t check_entry;
    uint32_t parent_cluster;
    uint32_t new_cluster;
    uint32_t free_cluster;
    uint32_t free_index;
    uint8_t child_sfn[FAT32_DIRECTORY_NAME_LENGTH];

    if ((fs_ctx == NULL) || (path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Resolve_Parent(fs_ctx, &cache, path, &parent_cluster, child_sfn);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = xFS_FAT32_Directory_Find_Entry(fs_ctx, &cache, parent_cluster, child_sfn, &check_entry, NULL, NULL);

    if (status == xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return xRETURN_xERR_xFS_ALREADY_EXISTS;
    }

    if (status != xRETURN_xERR_xFS_NOT_FOUND)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = xFS_FAT32_Allocate_Cluster(fs_ctx, &cache, &new_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = directory_zero_new_cluster(fs_ctx, &cache, new_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, &cache, new_cluster);
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = directory_write_dot_entries(fs_ctx, &cache, new_cluster, parent_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, &cache, new_cluster);
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    status = xFS_FAT32_Directory_Find_Free_Entry(fs_ctx, &cache, parent_cluster, &free_cluster, &free_index);

    if (status != xRETURN_OK)
    {
        (void)xFS_FAT32_Release_Chain(fs_ctx, &cache, new_cluster);
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    (void)memset(&entry, 0, sizeof(entry));
    (void)memcpy(entry.name, child_sfn, FAT32_DIRECTORY_NAME_LENGTH);
    entry.attributes = FAT32_ATTR_DIRECTORY;
    xWrite_LE16((uint8_t *)&entry.first_cluster_high, (uint16_t)((new_cluster >> 16U) & 0xFFFFU));
    xWrite_LE16((uint8_t *)&entry.first_cluster_low, (uint16_t)(new_cluster & 0xFFFFU));

    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, &cache, free_cluster, free_index, &entry);

    if (status == xRETURN_OK)
    {
        xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_DIR_CREATE, new_cluster);
    }

    (void)xFS_Cache_Invalidate(&cache);
    return status;
}

xRETURN_t xFS_Directory_Delete(xFS_Context_t *fs_ctx, const char *path)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint32_t dir_cluster;
    bool empty;

    if ((fs_ctx == NULL) || (path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Walk(fs_ctx, &cache, path, &entry, &entry_cluster, &entry_index);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    if ((entry.attributes & FAT32_ATTR_DIRECTORY) == 0U)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return xRETURN_xERR_xFS_NOT_DIRECTORY;
    }

    dir_cluster = fat32_entry_first_cluster(&entry);

    status = directory_is_empty(fs_ctx, &cache, dir_cluster, &empty);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    if (empty == false)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return xRETURN_xERR_xFS_NOT_EMPTY;
    }

    status = xFS_FAT32_Release_Chain(fs_ctx, &cache, dir_cluster);

    if (status != xRETURN_OK)
    {
        (void)xFS_Cache_Invalidate(&cache);
        return status;
    }

    entry.name[0U] = FAT32_DIRECTORY_ENTRY_FREE;
    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, &cache, entry_cluster, entry_index, &entry);

    if (status == xRETURN_OK)
    {
        xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_DIR_DELETE, dir_cluster);
    }

    (void)xFS_Cache_Invalidate(&cache);
    return status;
}

xRETURN_t xFS_Directory_Open(xFS_Context_t *fs_ctx, xFS_Directory_t *directory, const char *path)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t path_entry;
    uint32_t start_cluster;

    if ((fs_ctx == NULL) || (directory == NULL) || (path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    directory->fs_ctx = NULL;
    directory->start_cluster = 0U;
    directory->current_cluster = 0U;
    directory->current_index = 0U;
    directory->is_open = false;
    directory->is_end = true;

    status = xFS_Cache_Init(&directory->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    if (directory_path_is_root(path))
    {
        start_cluster = fs_ctx->root_dir_cluster;
    }
    else
    {
        status = xFS_Path_Walk(fs_ctx, &directory->cache, path, &path_entry, NULL, NULL);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if ((path_entry.attributes & FAT32_ATTR_DIRECTORY) == 0U)
        {
            return xRETURN_xERR_xFS_NOT_DIRECTORY;
        }

        start_cluster = fat32_entry_first_cluster(&path_entry);
    }

    if (!fat32_is_cluster_in_range(fs_ctx, start_cluster))
    {
        return xRETURN_xERR_xFS_CORRUPT;
    }

    directory->fs_ctx = fs_ctx;
    directory->start_cluster = start_cluster;
    directory->current_cluster = start_cluster;
    directory->current_index = 0U;
    directory->is_open = true;
    directory->is_end = false;

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_DIR_OPEN, start_cluster);

    return xRETURN_OK;
}

xRETURN_t xFS_Directory_Read(xFS_Directory_t *directory, xFS_Directory_Entry_t *entry, bool *has_entry)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t fat_entry;
    uint32_t entries_per_cluster;

    if ((directory == NULL) || (entry == NULL) || (has_entry == NULL) || (directory->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    *has_entry = false;

    if (directory->is_end == true)
    {
        return xRETURN_OK;
    }

    entries_per_cluster = fat32_entries_per_cluster(directory->fs_ctx);

    if (entries_per_cluster == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    while (directory->is_end == false)
    {
        if (directory->current_index >= entries_per_cluster)
        {
            status = directory_advance_cluster(directory);

            if (status != xRETURN_OK)
            {
                return status;
            }

            continue;
        }

        status = xFS_FAT32_Directory_Read_Entry(directory->fs_ctx, &directory->cache, directory->current_cluster, directory->current_index,
                                                &fat_entry);

        if (status != xRETURN_OK)
        {
            return status;
        }

        directory->current_index++;

        if (xFS_FAT32_Directory_Is_End(&fat_entry))
        {
            directory->is_end = true;
            return xRETURN_OK;
        }

        if ((xFS_FAT32_Directory_Is_Free(&fat_entry)) || (fat_entry.attributes == FAT32_ATTR_LFN))
        {
            continue;
        }

        directory_public_entry_from_fat32(&fat_entry, entry);
        *has_entry = true;
        return xRETURN_OK;
    }

    return xRETURN_OK;
}

xRETURN_t xFS_Directory_Rewind(xFS_Directory_t *directory)
{
    xRETURN_t status;

    if (directory == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (directory->is_open == false)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    status = xFS_Cache_Invalidate(&directory->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    directory->current_cluster = directory->start_cluster;
    directory->current_index = 0U;
    directory->is_end = false;

    return xRETURN_OK;
}

xRETURN_t xFS_Directory_Close(xFS_Directory_t *directory)
{
    if (directory == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (directory->is_open == true)
    {
        xFS_TRACE_E1(directory->fs_ctx, xFS_TRACE_CODE_DIR_CLOSE, directory->current_cluster);
    }

    directory->fs_ctx = NULL;
    directory->start_cluster = 0U;
    directory->current_cluster = 0U;
    directory->current_index = 0U;
    directory->is_open = false;
    directory->is_end = true;

    return xFS_Cache_Invalidate(&directory->cache);
}
// EOF /////////////////////////////////////////////////////////////////////////////
