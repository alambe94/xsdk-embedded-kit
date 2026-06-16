// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xboot_storage_fake.c
// @brief Host-testable memory-backed fake flash storage simulation implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_storage_fake.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t fake_read(void *storage_ctx, uint32_t offset, uint8_t *data, uint32_t length);
static xRETURN_t fake_write(void *storage_ctx, uint32_t offset, const uint8_t *data, uint32_t length);
static xRETURN_t fake_erase(void *storage_ctx, uint32_t offset, uint32_t length);
static xRETURN_t fake_flush(void *storage_ctx);

// STATIC DISPATCH TABLE
static const xBOOT_Storage_Ops_t g_fake_storage_ops = {
    .read = fake_read,
    .write = fake_write,
    .erase = fake_erase,
    .flush = fake_flush,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t fake_read(void *storage_ctx, uint32_t offset, uint8_t *data, uint32_t length)
{
    xBOOT_Storage_Fake_Context_t *ctx = (xBOOT_Storage_Fake_Context_t *)storage_ctx;

    if (ctx == NULL || ctx->buffer == NULL || data == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ctx->inject_read_fail)
    {
        return xRETURN_xERR_xBOOT_STORAGE_READ;
    }

    if ((offset + length) > ctx->size)
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    (void)memcpy(data, &ctx->buffer[offset], length);

    return xRETURN_xBOOT_OK;
}

static xRETURN_t fake_write(void *storage_ctx, uint32_t offset, const uint8_t *data, uint32_t length)
{
    xBOOT_Storage_Fake_Context_t *ctx = (xBOOT_Storage_Fake_Context_t *)storage_ctx;

    if (ctx == NULL || ctx->buffer == NULL || data == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ctx->inject_write_fail)
    {
        return xRETURN_xERR_xBOOT_STORAGE_WRITE;
    }

    if ((offset + length) > ctx->size)
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    // Verify flash behavior: bits can only be programmed from 1 to 0.
    // Transitioning a 0 back to a 1 without an erase is an error.
    for (uint32_t i = 0U; i < length; i++)
    {
        uint8_t old_val = ctx->buffer[offset + i];
        uint8_t write_val = data[i];

        if ((old_val & write_val) != write_val)
        {
            return xRETURN_xERR_xBOOT_STORAGE_WRITE; // Erase required before write
        }
    }

    for (uint32_t i = 0U; i < length; i++)
    {
        ctx->buffer[offset + i] &= data[i];
    }

    return xRETURN_xBOOT_OK;
}

static xRETURN_t fake_erase(void *storage_ctx, uint32_t offset, uint32_t length)
{
    xBOOT_Storage_Fake_Context_t *ctx = (xBOOT_Storage_Fake_Context_t *)storage_ctx;

    if (ctx == NULL || ctx->buffer == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ctx->inject_erase_fail)
    {
        return xRETURN_xERR_xBOOT_STORAGE_ERASE;
    }

    // Enforce alignment to physical sector bounds
    if (!xBOOT_IS_ALIGNED(offset, ctx->sector_size) || !xBOOT_IS_ALIGNED(length, ctx->sector_size))
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    if ((offset + length) > ctx->size)
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    (void)memset(&ctx->buffer[offset], 0xFF, length);

    return xRETURN_xBOOT_OK;
}

static xRETURN_t fake_flush(void *storage_ctx)
{
    (void)storage_ctx;
    return xRETURN_xBOOT_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

const xBOOT_Storage_Ops_t *xBOOT_Storage_Fake_Ops(void)
{
    return &g_fake_storage_ops;
}

xRETURN_t xBOOT_Storage_Fake_Init(xBOOT_Storage_Fake_Context_t *ctx, uint8_t *buffer, uint32_t size, uint32_t sector_size)
{
    if (ctx == NULL || buffer == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (size == 0U || sector_size == 0U || !xBOOT_IS_ALIGNED(size, sector_size))
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    ctx->buffer = buffer;
    ctx->size = size;
    ctx->sector_size = sector_size;
    ctx->inject_read_fail = false;
    ctx->inject_write_fail = false;
    ctx->inject_erase_fail = false;

    // Default buffer to erased state
    (void)memset(buffer, 0xFF, size);

    return xRETURN_xBOOT_OK;
}

void xBOOT_Storage_Fake_Reset_Errors(xBOOT_Storage_Fake_Context_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->inject_read_fail = false;
        ctx->inject_write_fail = false;
        ctx->inject_erase_fail = false;
    }
}

void xBOOT_Storage_Fake_Inject_Read_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable)
{
    if (ctx != NULL)
    {
        ctx->inject_read_fail = enable;
    }
}

void xBOOT_Storage_Fake_Inject_Write_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable)
{
    if (ctx != NULL)
    {
        ctx->inject_write_fail = enable;
    }
}

void xBOOT_Storage_Fake_Inject_Erase_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable)
{
    if (ctx != NULL)
    {
        ctx->inject_erase_fail = enable;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
