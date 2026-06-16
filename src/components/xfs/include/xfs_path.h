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

// @file xfs_path.h
// @brief FAT32 SFN path parsing and directory walking.

#ifndef XFS_PATH_H
#define XFS_PATH_H

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
#include "xfs_fat32_directory.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t size;
        uint8_t attributes;
        uint32_t first_cluster;
        bool is_directory;

    } xFS_Stat_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    static inline bool path_is_separator(char value)
    {
        return ((value == '/') || (value == '\\'));
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_Path_To_SFN(const char *path_component, uint32_t component_length, uint8_t sfn[FAT32_DIRECTORY_NAME_LENGTH]);

    xRETURN_t xFS_Path_Walk(xFS_Context_t *fs_ctx,
                            xFS_Cache_Entry_t *cache,
                            const char *path,
                            xFS_FAT32_Directory_Entry_t *entry,
                            uint32_t *entry_cluster,
                            uint32_t *entry_index);

    xRETURN_t xFS_Path_Resolve_Parent(xFS_Context_t *fs_ctx,
                                      xFS_Cache_Entry_t *cache,
                                      const char *path,
                                      uint32_t *parent_cluster,
                                      uint8_t child_name[FAT32_DIRECTORY_NAME_LENGTH]);

    xRETURN_t xFS_Path_Exists(xFS_Context_t *fs_ctx, const char *path, bool *exists);

    xRETURN_t xFS_Path_Stat(xFS_Context_t *fs_ctx, const char *path, xFS_Stat_t *stat);

    xRETURN_t xFS_Path_Rename(xFS_Context_t *fs_ctx, const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif // XFS_PATH_H
// EOF /////////////////////////////////////////////////////////////////////////////
