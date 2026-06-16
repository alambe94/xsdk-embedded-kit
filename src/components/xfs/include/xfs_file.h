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

// @file xfs_file.h
// @brief xFS file API.

#ifndef XFS_FILE_H
#define XFS_FILE_H

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

    typedef struct
    {
        xFS_Context_t *fs_ctx;
        xFS_Cache_Entry_t cache;

        uint32_t start_cluster;
        uint32_t current_cluster;
        uint32_t current_cluster_index;

        uint32_t file_size;
        uint32_t position;
        uint32_t directory_cluster;
        uint32_t directory_index;

        bool is_open;
        bool is_dirty;

    } xFS_File_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_File_Open(xFS_Context_t *fs_ctx, xFS_File_t *file, const char *path);

    xRETURN_t xFS_File_Create(xFS_Context_t *fs_ctx, xFS_File_t *file, const char *path);

    xRETURN_t xFS_File_Delete(xFS_Context_t *fs_ctx, const char *path);

    xRETURN_t xFS_File_Close(xFS_File_t *file);

    xRETURN_t xFS_File_Read(xFS_File_t *file, uint8_t *buffer, uint32_t length, uint32_t *bytes_read);

    xRETURN_t xFS_File_Write(xFS_File_t *file, const uint8_t *buffer, uint32_t length, uint32_t *bytes_written);

    xRETURN_t xFS_File_Truncate(xFS_File_t *file, uint32_t size);

    xRETURN_t xFS_File_Seek(xFS_File_t *file, uint32_t position);

    xRETURN_t xFS_File_Tell(const xFS_File_t *file, uint32_t *position);

    xRETURN_t xFS_File_Flush(xFS_File_t *file);

#ifdef __cplusplus
}
#endif

#endif // XFS_FILE_H
// EOF /////////////////////////////////////////////////////////////////////////////
