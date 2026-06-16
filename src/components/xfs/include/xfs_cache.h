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

// @file xfs_cache.h
// @brief Single-sector xFS cache interface.

#ifndef XFS_CACHE_H
#define XFS_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"
#include "xfs_defs.h"

    // Forward declaration - full definition in xfs_core.h.
    // Callers that pass fs_ctx by value or dereference its fields must include xfs_core.h.
    typedef struct xFS_Context_t xFS_Context_t;

    // MACROS //////////////////////////////////////////////////////////////////////

#define XFS_CACHE_SECTOR_SIZE XFS_SECTOR_SIZE

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xFS_Cache_Entry_t
    {
        bool is_valid;
        bool is_dirty;

        uint32_t sector;

        uint8_t buffer[XFS_CACHE_SECTOR_SIZE];

    } xFS_Cache_Entry_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_Cache_Init(xFS_Cache_Entry_t *cache);

    xRETURN_t xFS_Cache_Read(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t sector);

    xRETURN_t xFS_Cache_Write(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache);

    xRETURN_t xFS_Cache_Invalidate(xFS_Cache_Entry_t *cache);

#ifdef __cplusplus
}
#endif

#endif // XFS_CACHE_H
// EOF /////////////////////////////////////////////////////////////////////////////
