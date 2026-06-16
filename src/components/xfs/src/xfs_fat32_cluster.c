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

// @file xfs_fat32_cluster.c
// @brief FAT32 cluster-sector read and write helpers.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_fat32_cluster.h"
#include "xfs_fat32.h"
#include "xfs_config.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

uint32_t xFS_FAT32_Cluster_To_Sector(const xFS_Context_t *fs_ctx, uint32_t cluster)
{
    if (fs_ctx == NULL)
    {
        return 0U;
    }

    return fs_ctx->cluster_heap_start + ((cluster - 2U) * fs_ctx->sectors_per_cluster);
}

xRETURN_t xFS_FAT32_Cluster_Read(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster)
{
    uint32_t sector;

    if ((fs_ctx == NULL) || (cache == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    sector = xFS_FAT32_Cluster_To_Sector(fs_ctx, cluster);

    return xFS_Cache_Read(fs_ctx, cache, sector);
}

xRETURN_t xFS_FAT32_Cluster_Write(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster)
{
    uint32_t sector;

    if ((fs_ctx == NULL) || (cache == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if (!fat32_is_cluster_in_range(fs_ctx, cluster))
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    sector = xFS_FAT32_Cluster_To_Sector(fs_ctx, cluster);

    if ((cache->is_dirty == true) && (cache->is_valid == true) && (cache->sector != sector))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    cache->sector = sector;
    cache->is_valid = true;
    cache->is_dirty = true;

    return xFS_Cache_Write(fs_ctx, cache);
}
// EOF /////////////////////////////////////////////////////////////////////////////
