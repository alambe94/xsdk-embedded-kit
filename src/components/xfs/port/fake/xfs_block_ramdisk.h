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

// @file xfs_block_ramdisk.h
// @brief RAM-backed xFS block-device test backend.

#ifndef XFS_BLOCK_RAMDISK_H
#define XFS_BLOCK_RAMDISK_H

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

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint8_t *storage;

        uint32_t sector_size;
        uint32_t sector_count;

    } xFS_RAMDisk_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    extern xFS_Block_Driver_t gxFS_RAMDisk_Driver;

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XFS_BLOCK_RAMDISK_H
// EOF /////////////////////////////////////////////////////////////////////////////
