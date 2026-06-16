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

// @file xfs_block_device.h
// @brief xFS block-device driver interface.

#ifndef XFS_BLOCK_DEVICE_H
#define XFS_BLOCK_DEVICE_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ///////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "xfs_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        xRETURN_t (*init)(void *driver_ctx);

        xRETURN_t (*read_sector)(void *driver_ctx, uint32_t lba, uint8_t *buffer, uint32_t sector_count);

        xRETURN_t (*write_sector)(void *driver_ctx, const uint32_t lba, const uint8_t *buffer, uint32_t sector_count);

        xRETURN_t (*flush)(void *driver_ctx);

    } xFS_Block_Driver_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XFS_BLOCK_DEVICE_H
// EOF /////////////////////////////////////////////////////////////////////////////
