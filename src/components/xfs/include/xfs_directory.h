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

// @file xfs_directory.h
// @brief xFS directory iteration API.

#ifndef XFS_DIRECTORY_H
#define XFS_DIRECTORY_H

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

#define XFS_DIRECTORY_NAME_MAX 13U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        char name[XFS_DIRECTORY_NAME_MAX];
        uint8_t attributes;
        uint32_t first_cluster;
        uint32_t size;
        bool is_directory;

    } xFS_Directory_Entry_t;

    typedef struct
    {
        xFS_Context_t *fs_ctx;
        xFS_Cache_Entry_t cache;
        uint32_t start_cluster;
        uint32_t current_cluster;
        uint32_t current_index;
        bool is_open;
        bool is_end;

    } xFS_Directory_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_Directory_Create(xFS_Context_t *fs_ctx, const char *path);

    xRETURN_t xFS_Directory_Delete(xFS_Context_t *fs_ctx, const char *path);

    xRETURN_t xFS_Directory_Open(xFS_Context_t *fs_ctx, xFS_Directory_t *directory, const char *path);

    xRETURN_t xFS_Directory_Read(xFS_Directory_t *directory, xFS_Directory_Entry_t *entry, bool *has_entry);

    xRETURN_t xFS_Directory_Rewind(xFS_Directory_t *directory);

    xRETURN_t xFS_Directory_Close(xFS_Directory_t *directory);

#ifdef __cplusplus
}
#endif

#endif // XFS_DIRECTORY_H
// EOF /////////////////////////////////////////////////////////////////////////////
