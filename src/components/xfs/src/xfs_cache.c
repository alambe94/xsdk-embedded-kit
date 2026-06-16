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

// @file xfs_cache.c
// @brief Single-sector write-back cache for xFS block I/O.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include <string.h>

#include "xfs_cache.h"
#include "xfs_config.h"
#include "xfs_core.h"

#include "xfs_log.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_Cache_Init(xFS_Cache_Entry_t *cache)
{
    if (cache == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    cache->is_valid = false;
    cache->is_dirty = false;
    cache->sector = 0U;
    (void)memset(cache->buffer, 0, sizeof(cache->buffer));

    return xRETURN_OK;
}

xRETURN_t xFS_Cache_Read(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache, uint32_t sector)
{
    xRETURN_t status;

    if ((fs_ctx == NULL) || (cache == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if ((cache->is_valid == true) && (cache->sector == sector))
    {
        return xRETURN_OK;
    }

    if (cache->is_dirty == true)
    {
        status = xFS_Cache_Write(fs_ctx, cache);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    status = fs_ctx->driver->read_sector(fs_ctx->driver_ctx, sector, cache->buffer, 1U);

    if (status != xRETURN_OK)
    {
        xFS_LOG(xRETURN_xERR_xFS_IO, "read_sector failed");
        return status;
    }

    cache->sector = sector;

    cache->is_valid = true;
    cache->is_dirty = false;

    return xRETURN_OK;
}

xRETURN_t xFS_Cache_Write(xFS_Context_t *fs_ctx, xFS_Cache_Entry_t *cache)
{
    xRETURN_t status;

    if ((fs_ctx == NULL) || (cache == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    if ((cache->is_valid == false) || (cache->is_dirty == false))
    {
        return xRETURN_OK;
    }

    status = fs_ctx->driver->write_sector(fs_ctx->driver_ctx, cache->sector, cache->buffer, 1U);

    if (status != xRETURN_OK)
    {
        xFS_LOG(xRETURN_xERR_xFS_IO, "write_sector failed");
        return status;
    }

    cache->is_dirty = false;

    return xRETURN_OK;
}

xRETURN_t xFS_Cache_Invalidate(xFS_Cache_Entry_t *cache)
{
    if (cache == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    cache->is_valid = false;
    cache->is_dirty = false;

    cache->sector = 0U;

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
