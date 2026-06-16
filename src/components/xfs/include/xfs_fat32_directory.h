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

// @file xfs_fat32_directory.h
// @brief FAT32 directory entry types, constants, and inline helpers.

#ifndef XFS_FAT32_DIRECTORY_H
#define XFS_FAT32_DIRECTORY_H

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
#include "xfs_core.h"

    // Forward declaration - full definition in xfs_cache.h.
    // Only pointer parameters are used here; callers that access cache fields must include xfs_cache.h.
    typedef struct xFS_Cache_Entry_t xFS_Cache_Entry_t;

    // MACROS //////////////////////////////////////////////////////////////////////

#define FAT32_DIRECTORY_ENTRY_SIZE 32U

#define FAT32_DIRECTORY_ENTRY_FREE 0xE5U
#define FAT32_DIRECTORY_ENTRY_END  0x00U

#define FAT32_DIRECTORY_NAME_LENGTH 11U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct __attribute__((packed)) xFS_FAT32_Directory_Entry_t
    {
        uint8_t name[FAT32_DIRECTORY_NAME_LENGTH];

        uint8_t attributes;

        uint8_t reserved;
        uint8_t creation_time_tenths;

        uint16_t creation_time;
        uint16_t creation_date;

        uint16_t access_date;

        uint16_t first_cluster_high;

        uint16_t modification_time;
        uint16_t modification_date;

        uint16_t first_cluster_low;

        uint32_t file_size;

    } xFS_FAT32_Directory_Entry_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    static inline uint32_t fat32_entries_per_cluster(const xFS_Context_t *fs_ctx)
    {
        if ((fs_ctx == NULL) || (fs_ctx->bytes_per_sector == 0U) || (fs_ctx->sectors_per_cluster == 0U))
        {
            return 0U;
        }
        return (fs_ctx->bytes_per_sector / FAT32_DIRECTORY_ENTRY_SIZE) * fs_ctx->sectors_per_cluster;
    }

    static inline uint32_t fat32_entry_first_cluster(const xFS_FAT32_Directory_Entry_t *entry)
    {
        uint16_t high;
        uint16_t low;
        if (entry == NULL)
        {
            return 0U;
        }
        high = xRead_LE16((const uint8_t *)&entry->first_cluster_high);
        low = xRead_LE16((const uint8_t *)&entry->first_cluster_low);
        return (((uint32_t)high) << 16U) | (uint32_t)low;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_FAT32_Directory_Read_Entry(xFS_Context_t *fs_ctx,
                                             xFS_Cache_Entry_t *cache,
                                             uint32_t cluster,
                                             uint32_t entry_index,
                                             xFS_FAT32_Directory_Entry_t *entry);

    xRETURN_t xFS_FAT32_Directory_Write_Entry(xFS_Context_t *fs_ctx,
                                              xFS_Cache_Entry_t *cache,
                                              uint32_t cluster,
                                              uint32_t entry_index,
                                              const xFS_FAT32_Directory_Entry_t *entry);

    bool xFS_FAT32_Directory_Is_End(const xFS_FAT32_Directory_Entry_t *entry);

    bool xFS_FAT32_Directory_Is_Free(const xFS_FAT32_Directory_Entry_t *entry);

    xRETURN_t xFS_FAT32_Directory_Find_Entry(xFS_Context_t *fs_ctx,
                                             xFS_Cache_Entry_t *cache,
                                             uint32_t start_cluster,
                                             const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                             xFS_FAT32_Directory_Entry_t *entry,
                                             uint32_t *entry_cluster,
                                             uint32_t *entry_index);

    xRETURN_t xFS_FAT32_Directory_Find_Free_Entry(xFS_Context_t *fs_ctx,
                                                  xFS_Cache_Entry_t *cache,
                                                  uint32_t start_cluster,
                                                  uint32_t *entry_cluster,
                                                  uint32_t *entry_index);

    xRETURN_t xFS_FAT32_Directory_Create_File_Entry(xFS_Context_t *fs_ctx,
                                                    xFS_Cache_Entry_t *cache,
                                                    uint32_t parent_cluster,
                                                    const uint8_t name[FAT32_DIRECTORY_NAME_LENGTH],
                                                    uint32_t *entry_cluster,
                                                    uint32_t *entry_index);

#ifdef __cplusplus
}
#endif

#endif // XFS_FAT32_DIRECTORY_H
// EOF /////////////////////////////////////////////////////////////////////////////
