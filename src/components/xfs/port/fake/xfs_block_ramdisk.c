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

// @file xfs_block_ramdisk.c
// @brief RAM-backed xFS block-device implementation.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_block_ramdisk.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

static xRETURN_t ramdisk_init(void *driver_ctx);

static xRETURN_t ramdisk_read_sector(void *driver_ctx, uint32_t lba, uint8_t *buffer, uint32_t sector_count);

static xRETURN_t ramdisk_write_sector(void *driver_ctx, const uint32_t lba, const uint8_t *buffer, uint32_t sector_count);

static xRETURN_t ramdisk_flush(void *driver_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static xRETURN_t ramdisk_init(void *driver_ctx)
{
    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    return xRETURN_OK;
}

static xRETURN_t ramdisk_read_sector(void *driver_ctx, uint32_t lba, uint8_t *buffer, uint32_t sector_count)
{
    xFS_RAMDisk_Context_t *ramdisk_ctx;
    uint32_t offset;
    uint32_t size;
    uint32_t index;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    ramdisk_ctx = (xFS_RAMDisk_Context_t *)driver_ctx;

    if ((lba + sector_count) > ramdisk_ctx->sector_count)
    {
        return xRETURN_xERR_xFS_OUT_OF_RANGE;
    }

    offset = lba * ramdisk_ctx->sector_size;
    size = sector_count * ramdisk_ctx->sector_size;

    for (index = 0U; index < size; index++)
    {
        buffer[index] = ramdisk_ctx->storage[offset + index];
    }

    return xRETURN_OK;
}

static xRETURN_t ramdisk_write_sector(void *driver_ctx, const uint32_t lba, const uint8_t *buffer, uint32_t sector_count)
{
    xFS_RAMDisk_Context_t *ramdisk_ctx;
    uint32_t offset;
    uint32_t size;
    uint32_t index;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    ramdisk_ctx = (xFS_RAMDisk_Context_t *)driver_ctx;

    if ((lba + sector_count) > ramdisk_ctx->sector_count)
    {
        return xRETURN_xERR_xFS_OUT_OF_RANGE;
    }

    offset = lba * ramdisk_ctx->sector_size;
    size = sector_count * ramdisk_ctx->sector_size;

    for (index = 0U; index < size; index++)
    {
        ramdisk_ctx->storage[offset + index] = buffer[index];
    }

    return xRETURN_OK;
}

static xRETURN_t ramdisk_flush(void *driver_ctx)
{
    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xFS_Block_Driver_t gxFS_RAMDisk_Driver = {.init = ramdisk_init,
                                          .read_sector = ramdisk_read_sector,
                                          .write_sector = ramdisk_write_sector,
                                          .flush = ramdisk_flush};
// EOF /////////////////////////////////////////////////////////////////////////////
