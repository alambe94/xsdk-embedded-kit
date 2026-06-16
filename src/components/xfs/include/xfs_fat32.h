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

// @file xfs_fat32.h
// @brief FAT32 entry read/write, cluster allocation/release, and chain management.

#ifndef XFS_FAT32_H
#define XFS_FAT32_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"
#include "xfs_core.h"
#include "xfs_cache.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define FAT32_ENTRY_SIZE       4U
#define FAT32_CLUSTER_MIN      2U
#define FAT32_ENTRY_VALUE_MASK 0x0FFFFFFFUL

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    static inline bool fat32_is_cluster_in_range(const xFS_Context_t *fs_ctx, uint32_t cluster)
    {
        if (fs_ctx == NULL)
        {
            return false;
        }
        return ((cluster >= FAT32_CLUSTER_MIN) && (cluster < (FAT32_CLUSTER_MIN + fs_ctx->total_clusters)));
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_FAT32_Read_Entry(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster, uint32_t *value);

    xRETURN_t xFS_FAT32_Write_Entry(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster, uint32_t value);

    xRETURN_t xFS_FAT32_Find_Free_Cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t *cluster);

    xRETURN_t xFS_FAT32_Allocate_Cluster(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t *cluster);

    xRETURN_t xFS_FAT32_Allocate_Chain(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t length, uint32_t *first_cluster);

    xRETURN_t xFS_FAT32_Release_Chain(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t first_cluster);

#ifdef __cplusplus
}
#endif

#endif // XFS_FAT32_H
// EOF /////////////////////////////////////////////////////////////////////////////
