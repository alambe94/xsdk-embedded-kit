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

// @file xfs_core.c
// @brief xFS core - filesystem context init, mount, and unmount.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include <string.h>

#include "xfs_core.h"
#include "xfs_defs.h"
#include "xfs_cache.h"
#include "xfs_fat32.h"
#include "xfs_fat32_directory.h"
#include "xfs_fat32_bpb.h"
#include "xfs_trace.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static void volume_label_from_sfn(const uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH], char label[XFS_VOLUME_LABEL_MAX])
{
    uint32_t end;
    uint32_t i;

    end = FAT32_DIRECTORY_NAME_LENGTH;

    while ((end > 0U) && (sfn[end - 1U] == (uint8_t)' '))
    {
        end--;
    }

    for (i = 0U; i < end; i++)
    {
        label[i] = (char)sfn[i];
    }

    label[end] = '\0';
}

static xRETURN_t volume_find_label(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, char label[XFS_VOLUME_LABEL_MAX])
{
    xRETURN_t status;
    xFS_FAT32_Directory_Entry_t dir_entry;
    uint32_t cluster;
    uint32_t next_cluster;
    uint32_t index;
    uint32_t entries_per_cluster;

    entries_per_cluster = fat32_entries_per_cluster(fs_ctx);

    if (entries_per_cluster == 0U)
    {
        return xRETURN_OK;
    }

    cluster = fs_ctx->root_dir_cluster;

    while (fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        for (index = 0U; index < entries_per_cluster; index++)
        {
            status = xFS_FAT32_Directory_Read_Entry(fs_ctx, cache, cluster, index, &dir_entry);

            if (status != xRETURN_OK)
            {
                return status;
            }

            if (xFS_FAT32_Directory_Is_End(&dir_entry))
            {
                return xRETURN_OK;
            }

            if ((!xFS_FAT32_Directory_Is_Free(&dir_entry)) && (dir_entry.attributes == FAT32_ATTR_VOLUME_ID))
            {
                volume_label_from_sfn(dir_entry.name, label);
                return xRETURN_OK;
            }
        }

        status = xFS_FAT32_Read_Entry(fs_ctx, cache, cluster, &next_cluster);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (xFS_IS_EOC(next_cluster))
        {
            return xRETURN_OK;
        }

        cluster = next_cluster;
    }

    return xRETURN_xERR_xFS_CORRUPT;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_Init(xFS_Context_t *fs_ctx, xFS_Block_Driver_t *driver, void *driver_ctx)
{
    if ((fs_ctx == NULL) || (driver == NULL) || (driver->init == NULL) || (driver->read_sector == NULL) || (driver->write_sector == NULL) ||
        (driver->flush == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    fs_ctx->driver = driver;
    fs_ctx->driver_ctx = driver_ctx;

    fs_ctx->bytes_per_sector = 0U;
    fs_ctx->sectors_per_cluster = 0U;

    fs_ctx->fat_start_sector = 0U;
    fs_ctx->cluster_heap_start = 0U;
    fs_ctx->total_clusters = 0U;

    fs_ctx->root_dir_cluster = 0U;

    (void)memset(fs_ctx->boot_sector, 0, sizeof(fs_ctx->boot_sector));

    fs_ctx->is_mounted = false;
    fs_ctx->trace_ctx = NULL;

    return fs_ctx->driver->init(fs_ctx->driver_ctx);
}

xRETURN_t xFS_Trace_Init(xFS_Context_t *fs_ctx, xTRACE_Context_t *trace_ctx)
{
    if (fs_ctx == NULL)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    fs_ctx->trace_ctx = trace_ctx;

    return xRETURN_OK;
}

xRETURN_t xFS_Mount(xFS_Context_t *fs_ctx)
{
    xRETURN_t status;
    xFS_FAT32_BPB_t bpb;

    if (fs_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if ((fs_ctx->driver == NULL) || (fs_ctx->driver->read_sector == NULL))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    fs_ctx->bytes_per_sector = 0U;
    fs_ctx->sectors_per_cluster = 0U;
    fs_ctx->fat_start_sector = 0U;
    fs_ctx->cluster_heap_start = 0U;
    fs_ctx->total_clusters = 0U;
    fs_ctx->root_dir_cluster = 0U;
    fs_ctx->is_mounted = false;

    status = fs_ctx->driver->read_sector(fs_ctx->driver_ctx, 0U, fs_ctx->boot_sector, 1U);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_FAT32_BPB_Parse(&bpb, fs_ctx->boot_sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xFS_FAT32_BPB_Validate(&bpb, fs_ctx->boot_sector);

    if (status != xRETURN_OK)
    {
        return status;
    }

    fs_ctx->bytes_per_sector = bpb.bytes_per_sector;

    fs_ctx->sectors_per_cluster = bpb.sectors_per_cluster;

    fs_ctx->fat_start_sector = bpb.fat_start_sector;

    fs_ctx->cluster_heap_start = bpb.cluster_heap_start;
    fs_ctx->total_clusters = bpb.total_clusters;

    fs_ctx->root_dir_cluster = bpb.root_dir_cluster;

    fs_ctx->is_mounted = true;

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_MOUNT, fs_ctx->root_dir_cluster);

    return xRETURN_OK;
}

xRETURN_t xFS_Unmount(xFS_Context_t *fs_ctx)
{
    if (fs_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_UNMOUNT, 0U);

    fs_ctx->is_mounted = false;

    return xRETURN_OK;
}

xRETURN_t xFS_Volume_Get_Info(xFS_Context_t *fs_ctx, xFS_Volume_Info_t *info)
{
    xRETURN_t status;
    xFS_Cache_Entry_t cache;
    uint32_t cluster;
    uint32_t fat_value;
    uint32_t free_count;

    if ((fs_ctx == NULL) || (info == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    info->total_clusters = fs_ctx->total_clusters;
    info->bytes_per_sector = fs_ctx->bytes_per_sector;
    info->sectors_per_cluster = fs_ctx->sectors_per_cluster;
    info->bytes_per_cluster = fs_ctx->bytes_per_sector * fs_ctx->sectors_per_cluster;
    info->free_clusters = 0U;
    info->label[0] = '\0';

    status = xFS_Cache_Init(&cache);

    if (status != xRETURN_OK)
    {
        return status;
    }

    free_count = 0U;

    for (cluster = FAT32_CLUSTER_MIN; cluster < (FAT32_CLUSTER_MIN + fs_ctx->total_clusters); cluster++)
    {
        status = xFS_FAT32_Read_Entry(fs_ctx, &cache, cluster, &fat_value);

        if (status != xRETURN_OK)
        {
            (void)xFS_Cache_Invalidate(&cache);
            return status;
        }

        if ((fat_value & FAT32_ENTRY_VALUE_MASK) == FAT32_FREE_CLUSTER)
        {
            free_count++;
        }
    }

    info->free_clusters = free_count;

    status = volume_find_label(fs_ctx, &cache, info->label);

    (void)xFS_Cache_Invalidate(&cache);

    return status;
}

xRETURN_t xFS_Sync(xFS_Context_t *fs_ctx)
{
    xRETURN_t status;

    if (fs_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (fs_ctx->is_mounted == false)
    {
        return xRETURN_xERR_xFS_NOT_MOUNTED;
    }

    status = fs_ctx->driver->flush(fs_ctx->driver_ctx);

    if (status == xRETURN_OK)
    {
        xFS_TRACE_E1(fs_ctx, xFS_TRACE_CODE_SYNC, 0U);
    }

    return status;
}
// EOF /////////////////////////////////////////////////////////////////////////////
