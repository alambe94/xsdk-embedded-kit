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

// @file xfs_block_usbmsc.c
// @brief USB MSC-backed xFS block-device adapter implementation.

// INCLUDES ///////////////////////////////////////////////////////////////////

#include "xfs_block_usbmsc.h"
#include "xfs_defs.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

static xRETURN_t usbmsc_init(void *driver_ctx);
static xRETURN_t usbmsc_read_sector(void *driver_ctx, uint32_t lba, uint8_t *buffer, uint32_t sector_count);
static xRETURN_t usbmsc_write_sector(void *driver_ctx, const uint32_t lba, const uint8_t *buffer, uint32_t sector_count);
static xRETURN_t usbmsc_flush(void *driver_ctx);
static bool usbmsc_context_is_valid(const xFS_USBMSC_Context_t *usbmsc_ctx);
static xUSBH_MSC_Instance_t *usbmsc_instance_get(const xFS_USBMSC_Context_t *usbmsc_ctx);
static xRETURN_t usbmsc_wait_ready(xFS_USBMSC_Context_t *usbmsc_ctx, xUSBH_MSC_Instance_t *instance);
static xRETURN_t usbmsc_capacity_refresh(xFS_USBMSC_Context_t *usbmsc_ctx);
static xRETURN_t usbmsc_validate_io(const xFS_USBMSC_Context_t *usbmsc_ctx, uint32_t lba, uint32_t sector_count);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

static bool usbmsc_context_is_valid(const xFS_USBMSC_Context_t *usbmsc_ctx)
{
    return (usbmsc_ctx != NULL) && (usbmsc_ctx->msc_ctx != NULL) && (usbmsc_ctx->msc_ctx->host_ctx != NULL) && (usbmsc_ctx->poll != NULL) &&
           (usbmsc_ctx->max_poll_count > 0U) && (usbmsc_ctx->lun <= xUSBH_MSC_MAX_LUN);
}

static xUSBH_MSC_Instance_t *usbmsc_instance_get(const xFS_USBMSC_Context_t *usbmsc_ctx)
{
    uint32_t i;

    for (i = 0U; i < xUSBH_MSC_MAX_INSTANCES; i++)
    {
        if ((usbmsc_ctx->msc_ctx->instances[i].is_allocated == true) && (usbmsc_ctx->msc_ctx->instances[i].lun == usbmsc_ctx->lun))
        {
            return &usbmsc_ctx->msc_ctx->instances[i];
        }
    }

    return NULL;
}

static xRETURN_t usbmsc_wait_ready(xFS_USBMSC_Context_t *usbmsc_ctx, xUSBH_MSC_Instance_t *instance)
{
    uint32_t poll_count;

    for (poll_count = 0U; poll_count < usbmsc_ctx->max_poll_count; poll_count++)
    {
        if (instance->state == xUSBH_MSC_STATE_READY)
        {
            return (instance->error == xUSBH_MSC_ERROR_NONE) ? xRETURN_OK : xRETURN_xERR_xFS_IO;
        }
        if ((instance->state == xUSBH_MSC_STATE_ERROR) || (instance->state == xUSBH_MSC_STATE_RESET_RECOVERY))
        {
            return xRETURN_xERR_xFS_IO;
        }

        xRETURN_t status = usbmsc_ctx->poll(usbmsc_ctx->poll_ctx);
        if (status != xRETURN_OK)
        {
            return xRETURN_xERR_xFS_IO;
        }
    }

    return xRETURN_xERR_xFS_IO;
}

static xRETURN_t usbmsc_capacity_refresh(xFS_USBMSC_Context_t *usbmsc_ctx)
{
    xUSBH_MSC_Capacity_t capacity = {0};
    xUSBH_MSC_Instance_t *instance = usbmsc_instance_get(usbmsc_ctx);
    if (instance == NULL)
    {
        usbmsc_ctx->is_media_present = false;
        return xRETURN_xERR_xFS_IO;
    }

    xRETURN_t status = xUSBH_MSC_Read_Capacity(usbmsc_ctx->msc_ctx, usbmsc_ctx->lun, &capacity);
    if (status != xRETURN_OK)
    {
        usbmsc_ctx->is_media_present = false;
        return xRETURN_xERR_xFS_IO;
    }

    status = usbmsc_wait_ready(usbmsc_ctx, instance);
    if (status != xRETURN_OK)
    {
        usbmsc_ctx->is_media_present = false;
        return status;
    }

    if ((capacity.block_size != XFS_SECTOR_SIZE) || (capacity.block_count == 0U))
    {
        usbmsc_ctx->is_media_present = false;
        return xRETURN_xERR_xFS_INVALID_VOLUME;
    }

    usbmsc_ctx->sector_count = capacity.block_count;
    usbmsc_ctx->sector_size = capacity.block_size;
    usbmsc_ctx->is_media_present = true;

    return xRETURN_OK;
}

static xRETURN_t usbmsc_validate_io(const xFS_USBMSC_Context_t *usbmsc_ctx, uint32_t lba, uint32_t sector_count)
{
    if ((usbmsc_ctx->is_initialized == false) || (usbmsc_ctx->is_media_present == false))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }
    if (sector_count == 0U)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }
    if (sector_count > UINT16_MAX)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }
    if ((lba > usbmsc_ctx->sector_count) || (sector_count > (usbmsc_ctx->sector_count - lba)))
    {
        return xRETURN_xERR_xFS_OUT_OF_RANGE;
    }

    return xRETURN_OK;
}

static xRETURN_t usbmsc_init(void *driver_ctx)
{
    xFS_USBMSC_Context_t *usbmsc_ctx = (xFS_USBMSC_Context_t *)driver_ctx;

    if (usbmsc_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }
    if (usbmsc_context_is_valid(usbmsc_ctx) == false)
    {
        return xRETURN_xERR_xFS_INVALID_ARGUMENT;
    }

    usbmsc_ctx->is_initialized = false;
    usbmsc_ctx->is_media_present = false;
    usbmsc_ctx->sector_count = 0U;
    usbmsc_ctx->sector_size = 0U;

    xRETURN_t status = usbmsc_capacity_refresh(usbmsc_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    usbmsc_ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t usbmsc_read_sector(void *driver_ctx, uint32_t lba, uint8_t *buffer, uint32_t sector_count)
{
    xFS_USBMSC_Context_t *usbmsc_ctx = (xFS_USBMSC_Context_t *)driver_ctx;

    if ((usbmsc_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    xRETURN_t status = usbmsc_validate_io(usbmsc_ctx, lba, sector_count);
    if (status != xRETURN_OK)
    {
        return status;
    }

    xUSBH_MSC_Instance_t *instance = usbmsc_instance_get(usbmsc_ctx);
    if (instance == NULL)
    {
        usbmsc_ctx->is_media_present = false;
        return xRETURN_xERR_xFS_IO;
    }

    status =
        xUSBH_MSC_Read_Blocks(usbmsc_ctx->msc_ctx, usbmsc_ctx->lun, lba, (uint16_t)sector_count, buffer, sector_count * XFS_SECTOR_SIZE);
    if (status != xRETURN_OK)
    {
        return xRETURN_xERR_xFS_IO;
    }

    return usbmsc_wait_ready(usbmsc_ctx, instance);
}

static xRETURN_t usbmsc_write_sector(void *driver_ctx, const uint32_t lba, const uint8_t *buffer, uint32_t sector_count)
{
    xFS_USBMSC_Context_t *usbmsc_ctx = (xFS_USBMSC_Context_t *)driver_ctx;

    if ((usbmsc_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    xRETURN_t status = usbmsc_validate_io(usbmsc_ctx, lba, sector_count);
    if (status != xRETURN_OK)
    {
        return status;
    }

    xUSBH_MSC_Instance_t *instance = usbmsc_instance_get(usbmsc_ctx);
    if (instance == NULL)
    {
        usbmsc_ctx->is_media_present = false;
        return xRETURN_xERR_xFS_IO;
    }

    status =
        xUSBH_MSC_Write_Blocks(usbmsc_ctx->msc_ctx, usbmsc_ctx->lun, lba, (uint16_t)sector_count, buffer, sector_count * XFS_SECTOR_SIZE);
    if (status != xRETURN_OK)
    {
        return xRETURN_xERR_xFS_IO;
    }

    return usbmsc_wait_ready(usbmsc_ctx, instance);
}

static xRETURN_t usbmsc_flush(void *driver_ctx)
{
    xFS_USBMSC_Context_t *usbmsc_ctx = (xFS_USBMSC_Context_t *)driver_ctx;

    if (usbmsc_ctx == NULL)
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }
    if ((usbmsc_ctx->is_initialized == false) || (usbmsc_ctx->is_media_present == false))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////

xRETURN_t xFS_USBMSC_Is_Ready(const xFS_USBMSC_Context_t *usbmsc_ctx, bool *is_ready)
{
    if ((usbmsc_ctx == NULL) || (is_ready == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }

    *is_ready = (usbmsc_ctx->is_initialized == true) && (usbmsc_ctx->is_media_present == true) && (usbmsc_instance_get(usbmsc_ctx) != NULL);

    return xRETURN_OK;
}

xRETURN_t xFS_USBMSC_Get_Capacity(const xFS_USBMSC_Context_t *usbmsc_ctx, xUSBH_MSC_Capacity_t *capacity)
{
    if ((usbmsc_ctx == NULL) || (capacity == NULL))
    {
        return xRETURN_xERR_xFS_NULL_POINTER;
    }
    if ((usbmsc_ctx->is_initialized == false) || (usbmsc_ctx->is_media_present == false))
    {
        return xRETURN_xERR_xFS_INVALID_STATE;
    }

    capacity->block_count = usbmsc_ctx->sector_count;
    capacity->block_size = usbmsc_ctx->sector_size;

    return xRETURN_OK;
}

xFS_Block_Driver_t gxFS_USBMSC_Driver = {.init = usbmsc_init,
                                         .read_sector = usbmsc_read_sector,
                                         .write_sector = usbmsc_write_sector,
                                         .flush = usbmsc_flush};
// EOF /////////////////////////////////////////////////////////////////////////////
