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

// @file xfs_format.h
// @brief xFS FAT32 volume formatting API.

#ifndef XFS_FORMAT_H
#define XFS_FORMAT_H

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

    // MACROS //////////////////////////////////////////////////////////////////////

#define XFS_FORMAT_VOLUME_LABEL_LENGTH 11U
#define XFS_FORMAT_DEFAULT_SECTOR_SIZE 512U
#define XFS_FORMAT_DEFAULT_VOLUME_ID   0x58465331UL

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint32_t bytes_per_sector;
        uint32_t sector_count;
        uint32_t sectors_per_cluster;
        uint32_t reserved_sector_count;
        uint32_t fat_count;
        uint32_t fat_size;
        uint32_t root_dir_cluster;
        uint32_t volume_id;
        uint8_t volume_label[XFS_FORMAT_VOLUME_LABEL_LENGTH];

    } xFS_Format_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xFS_Format_Config_Default(xFS_Format_Config_t *config, uint32_t sector_count);

    xRETURN_t xFS_Format_FAT32(xFS_Block_Driver_t *driver, void *driver_ctx, const xFS_Format_Config_t *config);

#ifdef __cplusplus
}
#endif

#endif // XFS_FORMAT_H
// EOF /////////////////////////////////////////////////////////////////////////////
