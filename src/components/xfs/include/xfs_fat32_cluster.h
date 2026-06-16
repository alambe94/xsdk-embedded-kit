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

// @file xfs_fat32_cluster.h
// @brief FAT32 cluster-to-sector helpers.

#ifndef XFS_FAT32_CLUSTER_H
#define XFS_FAT32_CLUSTER_H

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

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    uint32_t xFS_FAT32_Cluster_To_Sector(const xFS_Context_t *fs_ctx, uint32_t cluster);

    xRETURN_t xFS_FAT32_Cluster_Read(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster);

    xRETURN_t xFS_FAT32_Cluster_Write(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t cluster);

#ifdef __cplusplus
}
#endif

#endif // XFS_FAT32_CLUSTER_H
// EOF /////////////////////////////////////////////////////////////////////////////
