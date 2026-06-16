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

// @file xfs_file.c
// @brief xFS file API.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_file.h"
#include "xfs_defs.h"
#include "xfs_fat32.h"
#include "xfs_fat32_cluster.h"
#include "xfs_fat32_directory.h"
#include "xfs_path.h"
#include "xfs_trace.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static uint32_t file_cluster_size(const xFS_File_t *file)
{
    if ((file == NULL) || (file->fs_ctx == NULL))
    {
        return 0U;
    }

    return file->fs_ctx->bytes_per_sector * file->fs_ctx->sectors_per_cluster;
}

static uint32_t file_directory_entry_size(const xFS_FAT32_Directory_Entry_t *entry)
{
    if (entry == NULL)
    {
        return 0U;
    }

    return xRead_LE32((const uint8_t *)&entry->file_size);
}

static void file_directory_entry_set_first_cluster(xFS_FAT32_Directory_Entry_t *entry, uint32_t cluster)
{
    if (entry == NULL)
    {
        return;
    }

    xWrite_LE16((uint8_t *)&entry->first_cluster_high, (uint16_t)((cluster >> 16U) & 0xFFFFU));
    xWrite_LE16((uint8_t *)&entry->first_cluster_low, (uint16_t)(cluster & 0xFFFFU));
}

static void file_directory_entry_set_size(xFS_FAT32_Directory_Entry_t *entry, uint32_t size)
{
    if (entry == NULL)
    {
        return;
    }

    xWrite_LE32((uint8_t *)&entry->file_size, size);
}

static uint32_t file_required_cluster_count(uint32_t size, uint32_t cluster_size)
{
    if ((size == 0U) || (cluster_size == 0U))
    {
        return 0U;
    }

    return ((size - 1U) / cluster_size) + 1U;
}

static xRETURN_t file_move_to_cluster_index(xFS_File_t *file, uint32_t target_cluster_index)
{
    xRETURN_t status;
    uint32_t cluster_index;
    uint32_t next_cluster;

    if ((file == NULL) || (file->fs_ctx == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (target_cluster_index < file->current_cluster_index)
    {
        file->current_cluster = file->start_cluster;
        file->current_cluster_index = 0U;
    }

    cluster_index = file->current_cluster_index;

    while (cluster_index < target_cluster_index)
    {
        status = xFS_FAT32_Read_Entry(file->fs_ctx, &file->cache, file->current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return xRETURN_xERR_xFS_CORRUPT;
        }

        file->current_cluster = next_cluster;
        cluster_index++;
    }

    file->current_cluster_index = target_cluster_index;

    return xRETURN_OK;
}

static xRETURN_t file_shrink_to_size(xFS_File_t *file, uint32_t size)
{
    xRETURN_t status;
    uint32_t cluster_size;
    uint32_t required_clusters;
    uint32_t retained_cluster;
    uint32_t released_cluster;

    if ((file == NULL) || (file->fs_ctx == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    cluster_size = file_cluster_size(file);

    if (cluster_size == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    required_clusters = file_required_cluster_count(size, cluster_size);

    if (required_clusters == 0U)
    {
        if (xFS_IS_VALID_CLUSTER(file->start_cluster))
        {
            status = xFS_FAT32_Release_Chain(file->fs_ctx, &file->cache, file->start_cluster);

            if (status != xRETURN_OK)
            {
                return status;
            }
        }

        file->start_cluster = 0U;
        file->current_cluster = 0U;
        file->current_cluster_index = 0U;
    }
    else
    {
        status = file_move_to_cluster_index(file, required_clusters - 1U);

        if (status != xRETURN_OK)
        {
            return status;
        }

        retained_cluster = file->current_cluster;

        status = xFS_FAT32_Read_Entry(file->fs_ctx, &file->cache, retained_cluster, &released_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (!xFS_IS_EOC(released_cluster))
        {
            status = xFS_FAT32_Write_Entry(file->fs_ctx, &file->cache, retained_cluster, FAT32_EOC_MIN);

            if (status != xRETURN_OK)
            {
                return status;
            }

            status = xFS_Cache_Write(file->fs_ctx, &file->cache);

            if (status != xRETURN_OK)
            {
                return status;
            }

            status = xFS_FAT32_Release_Chain(file->fs_ctx, &file->cache, released_cluster);

            if (status != xRETURN_OK)
            {
                return status;
            }
        }

        file->current_cluster = retained_cluster;
        file->current_cluster_index = required_clusters - 1U;
    }

    file->file_size = size;

    if (file->position > file->file_size)
    {
        file->position = file->file_size;
    }

    file->is_dirty = true;

    return xRETURN_OK;
}

static xRETURN_t file_move_to_cluster_index_for_write(xFS_File_t *file, uint32_t target_cluster_index)
{
    xRETURN_t status;
    uint32_t cluster_index;
    uint32_t next_cluster;
    uint32_t new_cluster;

    if ((file == NULL) || (file->fs_ctx == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (!xFS_IS_VALID_CLUSTER(file->start_cluster))
    {
        status = xFS_FAT32_Allocate_Cluster(file->fs_ctx, &file->cache, &new_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        file->start_cluster = new_cluster;
        file->current_cluster = new_cluster;
        file->current_cluster_index = 0U;
        file->is_dirty = true;
    }

    if (target_cluster_index < file->current_cluster_index)
    {
        file->current_cluster = file->start_cluster;
        file->current_cluster_index = 0U;
    }

    cluster_index = file->current_cluster_index;

    while (cluster_index < target_cluster_index)
    {
        status = xFS_FAT32_Read_Entry(file->fs_ctx, &file->cache, file->current_cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            status = xFS_FAT32_Allocate_Cluster(file->fs_ctx, &file->cache, &new_cluster);

            if (status != xRETURN_OK)
            {
                return status;
            }

            status = xFS_FAT32_Write_Entry(file->fs_ctx, &file->cache, file->current_cluster, new_cluster);

            if (status != xRETURN_OK)
            {
                return status;
            }

            status = xFS_Cache_Write(file->fs_ctx, &file->cache);

            if (status != xRETURN_OK)
            {
                return status;
            }

            next_cluster = new_cluster;
        }

        file->current_cluster = next_cluster;
        cluster_index++;
    }

    file->current_cluster_index = target_cluster_index;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_File_Open(xFS_Context_t *fs_ctx, xFS_File_t *file, const char *path)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t start_cluster;
    uint32_t entry_cluster;
    uint32_t entry_index;

    if ((fs_ctx == NULL) || (file == NULL) || (path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    file->fs_ctx = NULL;
    file->start_cluster = 0U;
    file->current_cluster = 0U;
    file->current_cluster_index = 0U;
    file->file_size = 0U;
    file->position = 0U;
    file->directory_cluster = 0U;
    file->directory_index = 0U;
    file->is_open = false;
    file->is_dirty = false;

    status = xFS_Cache_Init(&file->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Walk(fs_ctx, &file->cache, path, &entry, &entry_cluster, &entry_index);

    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((entry.attributes & FAT32_ATTR_DIRECTORY) != 0U)
    {
        return xRETURN_xERR_xFS_NOT_FILE;
    }

    start_cluster = fat32_entry_first_cluster(&entry);

    file->fs_ctx = fs_ctx;
    file->start_cluster = start_cluster;
    file->current_cluster = start_cluster;
    file->current_cluster_index = 0U;
    file->file_size = file_directory_entry_size(&entry);
    file->position = 0U;
    file->directory_cluster = entry_cluster;
    file->directory_index = entry_index;
    file->is_open = true;
    file->is_dirty = false;

    if ((file->file_size > 0U) && !xFS_IS_VALID_CLUSTER(file->start_cluster))
    {
        file->is_open = false;
        return xRETURN_xERR_xFS_CORRUPT;
    }

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_FILE_OPEN, start_cluster);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Create(xFS_Context_t *fs_ctx, xFS_File_t *file, const char *path)
{
    xRETURN_t status;
    uint32_t parent_cluster;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint8_t name[FAT32_DIRECTORY_NAME_LENGTH];

    if ((fs_ctx == NULL) || (file == NULL) || (path == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    file->fs_ctx = NULL;
    file->start_cluster = 0U;
    file->current_cluster = 0U;
    file->current_cluster_index = 0U;
    file->file_size = 0U;
    file->position = 0U;
    file->directory_cluster = 0U;
    file->directory_index = 0U;
    file->is_open = false;
    file->is_dirty = false;

    status = xFS_Cache_Init(&file->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Path_Resolve_Parent(fs_ctx, &file->cache, path, &parent_cluster, name);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_FAT32_Directory_Create_File_Entry(fs_ctx, &file->cache, parent_cluster, name, &entry_cluster, &entry_index);

    if (status != xRETURN_OK)
    {
        return status;
    }

    file->fs_ctx = fs_ctx;
    file->start_cluster = 0U;
    file->current_cluster = 0U;
    file->current_cluster_index = 0U;
    file->file_size = 0U;
    file->position = 0U;
    file->directory_cluster = entry_cluster;
    file->directory_index = entry_index;
    file->is_open = true;
    file->is_dirty = false;

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_FILE_CREATE, entry_cluster);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Delete(xFS_Context_t *fs_ctx, const char *path)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    xFS_FAT32_Directory_Entry_t entry;
    uint32_t entry_cluster;
    uint32_t entry_index;
    uint32_t first_cluster;

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
        return status;
    }

    if ((entry.attributes & FAT32_ATTR_DIRECTORY) != 0U)
    {
        return xRETURN_xERR_xFS_NOT_FILE;
    }

    first_cluster = fat32_entry_first_cluster(&entry);

    entry.name[0U] = FAT32_DIRECTORY_ENTRY_FREE;

    status = xFS_FAT32_Directory_Write_Entry(fs_ctx, &cache, entry_cluster, entry_index, &entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    if (xFS_IS_VALID_CLUSTER(first_cluster))
    {
        status = xFS_FAT32_Release_Chain(fs_ctx, &cache, first_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_FILE_DELETE, first_cluster);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Close(xFS_File_t *file)
{
    xRETURN_t status;

    if (file == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (file->is_open == true)
    {
        status = xFS_File_Flush(file);

        if (status != xRETURN_OK)
        {
            return status;
        }

        xFS_TRACE_E1(file->fs_ctx, xFS_TRACE_CODE_FILE_CLOSE, file->start_cluster);
    }

    file->fs_ctx = NULL;
    file->start_cluster = 0U;
    file->current_cluster = 0U;
    file->current_cluster_index = 0U;
    file->file_size = 0U;
    file->position = 0U;
    file->directory_cluster = 0U;
    file->directory_index = 0U;
    file->is_open = false;
    file->is_dirty = false;

    return xFS_Cache_Invalidate(&file->cache);
}

xRETURN_t xFS_File_Read(xFS_File_t *file, uint8_t *buffer, uint32_t length, uint32_t *bytes_read)
{
    xRETURN_t status;
    uint32_t cluster_size;
    uint32_t target_cluster_index;
    uint32_t sector;
    uint32_t sector_offset;
    uint32_t bytes_available;
    uint32_t bytes_to_copy;
    uint32_t index;

    if ((file == NULL) || (buffer == NULL) || (bytes_read == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    *bytes_read = 0U;
    cluster_size = file_cluster_size(file);

    if (cluster_size == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    while ((*bytes_read < length) && (file->position < file->file_size))
    {
        target_cluster_index = file->position / cluster_size;

        status = file_move_to_cluster_index(file, target_cluster_index);

        if (status != xRETURN_OK)
        {
            return status;
        }

        sector = xFS_FAT32_Cluster_To_Sector(file->fs_ctx, file->current_cluster);
        sector += (file->position % cluster_size) / file->fs_ctx->bytes_per_sector;
        sector_offset = file->position % file->fs_ctx->bytes_per_sector;

        status = xFS_Cache_Read(file->fs_ctx, &file->cache, sector);

        if (status != xRETURN_OK)
        {
            return status;
        }

        bytes_available = file->fs_ctx->bytes_per_sector - sector_offset;

        if (bytes_available > (length - *bytes_read))
        {
            bytes_available = length - *bytes_read;
        }

        if (bytes_available > (file->file_size - file->position))
        {
            bytes_available = file->file_size - file->position;
        }

        bytes_to_copy = bytes_available;

        for (index = 0U; index < bytes_to_copy; index++)
        {
            buffer[*bytes_read + index] = file->cache.buffer[sector_offset + index];
        }

        *bytes_read += bytes_to_copy;
        file->position += bytes_to_copy;
    }

    xFS_TRACE_E1(file->fs_ctx, xFS_TRACE_CODE_FILE_READ, *bytes_read);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Write(xFS_File_t *file, const uint8_t *buffer, uint32_t length, uint32_t *bytes_written)
{
    xRETURN_t status;
    uint32_t cluster_size;
    uint32_t target_cluster_index;
    uint32_t sector;
    uint32_t sector_offset;
    uint32_t bytes_available;
    uint32_t bytes_to_copy;
    uint32_t index;

    if ((file == NULL) || (buffer == NULL) || (bytes_written == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    *bytes_written = 0U;
    cluster_size = file_cluster_size(file);

    if (cluster_size == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    while (*bytes_written < length)
    {
        target_cluster_index = file->position / cluster_size;

        status = file_move_to_cluster_index_for_write(file, target_cluster_index);

        if (status != xRETURN_OK)
        {
            return status;
        }

        sector = xFS_FAT32_Cluster_To_Sector(file->fs_ctx, file->current_cluster);
        sector += (file->position % cluster_size) / file->fs_ctx->bytes_per_sector;
        sector_offset = file->position % file->fs_ctx->bytes_per_sector;

        status = xFS_Cache_Read(file->fs_ctx, &file->cache, sector);

        if (status != xRETURN_OK)
        {
            return status;
        }

        bytes_available = file->fs_ctx->bytes_per_sector - sector_offset;

        if (bytes_available > (length - *bytes_written))
        {
            bytes_available = length - *bytes_written;
        }

        bytes_to_copy = bytes_available;

        for (index = 0U; index < bytes_to_copy; index++)
        {
            file->cache.buffer[sector_offset + index] = buffer[*bytes_written + index];
        }

        file->cache.is_dirty = true;
        *bytes_written += bytes_to_copy;
        file->position += bytes_to_copy;

        if (file->position > file->file_size)
        {
            file->file_size = file->position;
            file->is_dirty = true;
        }
    }

    xFS_TRACE_E1(file->fs_ctx, xFS_TRACE_CODE_FILE_WRITE, *bytes_written);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Truncate(xFS_File_t *file, uint32_t size)
{
    xRETURN_t status;
    uint8_t zero_buffer[32U] = {0};
    uint32_t original_position;
    uint32_t bytes_written;
    uint32_t bytes_to_write;

    if ((file == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (size == file->file_size)
    {
        return xRETURN_OK;
    }

    if (size < file->file_size)
    {
        return file_shrink_to_size(file, size);
    }

    original_position = file->position;
    file->position = file->file_size;

    while (file->file_size < size)
    {
        bytes_to_write = size - file->file_size;

        if (bytes_to_write > sizeof(zero_buffer))
        {
            bytes_to_write = sizeof(zero_buffer);
        }

        status = xFS_File_Write(file, zero_buffer, bytes_to_write, &bytes_written);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (bytes_written != bytes_to_write)
        {
            return xRETURN_xERR_xFS_IO;
        }
    }

    return xFS_File_Seek(file, original_position);
}

xRETURN_t xFS_File_Seek(xFS_File_t *file, uint32_t position)
{
    xRETURN_t status;
    uint32_t cluster_size;
    uint32_t target_cluster_index;

    if ((file == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    if (position > file->file_size)
    {
        return xRETURN_xERR_xFS_OUT_OF_RANGE;
    }

    status = xFS_Cache_Write(file->fs_ctx, &file->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_Cache_Invalidate(&file->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    cluster_size = file_cluster_size(file);

    if (cluster_size == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    if (position == file->file_size)
    {
        file->position = position;
        xFS_TRACE_E1(file->fs_ctx, xFS_TRACE_CODE_FILE_SEEK, position);
        return xRETURN_OK;
    }

    target_cluster_index = position / cluster_size;
    status = file_move_to_cluster_index(file, target_cluster_index);

    if (status != xRETURN_OK)
    {
        return status;
    }

    file->position = position;

    xFS_TRACE_E1(file->fs_ctx, xFS_TRACE_CODE_FILE_SEEK, position);

    return xRETURN_OK;
}

xRETURN_t xFS_File_Tell(const xFS_File_t *file, uint32_t *position)
{
    if ((file == NULL) || (position == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    *position = file->position;

    return xRETURN_OK;
}

xRETURN_t xFS_File_Flush(xFS_File_t *file)
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t entry;

    if ((file == NULL) || (file->is_open == false))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    status = xFS_Cache_Write(file->fs_ctx, &file->cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    if (file->is_dirty == false)
    {
        return xRETURN_OK;
    }

    status = xFS_FAT32_Directory_Read_Entry(file->fs_ctx, &file->cache, file->directory_cluster, file->directory_index, &entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    file_directory_entry_set_first_cluster(&entry, file->start_cluster);
    file_directory_entry_set_size(&entry, file->file_size);

    status = xFS_FAT32_Directory_Write_Entry(file->fs_ctx, &file->cache, file->directory_cluster, file->directory_index, &entry);

    if (status != xRETURN_OK)
    {
        return status;
    }

    file->is_dirty = false;

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
