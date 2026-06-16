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

// @file xfs_core.h
// @brief xFS public API - filesystem context, mount, and unmount.

#ifndef XFS_CORE_H
#define XFS_CORE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"
#include "xfs_block_device.h"
#include "xfs_defs.h"
#include "xtrace.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define XFS_VOLUME_LABEL_MAX 12U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t total_clusters;
        uint32_t free_clusters;
        uint32_t bytes_per_sector;
        uint32_t sectors_per_cluster;
        uint32_t bytes_per_cluster;
        char label[XFS_VOLUME_LABEL_MAX];

    } xFS_Volume_Info_t;

    typedef struct xFS_Context_t
    {
        xFS_Block_Driver_t *driver;

        void *driver_ctx;

        uint32_t bytes_per_sector;
        uint32_t sectors_per_cluster;

        uint32_t fat_start_sector;
        uint32_t cluster_heap_start;
        uint32_t total_clusters;

        uint32_t root_dir_cluster;

        uint8_t boot_sector[XFS_SECTOR_SIZE];

        bool is_mounted;

        xTRACE_Context_t *trace_ctx;

    } xFS_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the filesystem context and invoke driver->init. Must be called
    // before xFS_Mount. Does not read media - safe to call even if no media is present.
    xRETURN_t xFS_Init(xFS_Context_t *fs_ctx, xFS_Block_Driver_t *driver, void *driver_ctx);

    // Attach or detach an optional xTRACE context for filesystem event tracing.
    // Passing NULL for trace_ctx disables trace emission for this filesystem context.
    xRETURN_t xFS_Trace_Init(xFS_Context_t *fs_ctx, xTRACE_Context_t *trace_ctx);

    // Read and validate the FAT32 boot sector, populate layout fields, and set
    // is_mounted. Returns an error if the media is absent, unformatted, or not FAT32.
    xRETURN_t xFS_Mount(xFS_Context_t *fs_ctx);

    // Mark the volume as unmounted. Does NOT flush dirty cache or close open files.
    // Callers must close all xFS_File_t and xFS_Directory_t handles and call
    // driver->flush before calling xFS_Unmount; otherwise dirty data will be lost.
    //
    // Teardown / Deinit path: xFS has no separate xFS_Deinit because Unmount is the
    // natural teardown boundary. After xFS_Unmount returns, the caller may zero
    // xFS_Context_t and reuse it for a subsequent xFS_Init + xFS_Mount cycle without
    // any additional cleanup. The block driver's own deinit (if it has one) is the
    // caller's responsibility and is outside xFS scope.
    xRETURN_t xFS_Unmount(xFS_Context_t *fs_ctx);

    // Return volume layout, free cluster count, and label. Scans the FAT to
    // count free clusters; this is O(total_clusters) and reads all FAT sectors.
    xRETURN_t xFS_Volume_Get_Info(xFS_Context_t *fs_ctx, xFS_Volume_Info_t *info);

    // Request the block driver to flush any internally buffered writes to the
    // physical medium. Open file handles must be flushed by the caller before
    // invoking xFS_Sync; this function cannot access caller-owned file handles.
    xRETURN_t xFS_Sync(xFS_Context_t *fs_ctx);

#ifdef __cplusplus
}
#endif

#endif // XFS_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
