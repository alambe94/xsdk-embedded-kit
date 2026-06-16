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

// @file xboot_storage.c
// @brief Storage operations helper API implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_storage.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBOOT_Storage_Read(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, uint8_t *data, uint32_t length)
{
    if (ops == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ops->read == NULL)
    {
        return xRETURN_xERR_xBOOT_UNSUPPORTED;
    }

    return ops->read(storage_ctx, offset, data, length);
}

xRETURN_t xBOOT_Storage_Write(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, const uint8_t *data, uint32_t length)
{
    if (ops == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ops->write == NULL)
    {
        return xRETURN_xERR_xBOOT_UNSUPPORTED;
    }

    return ops->write(storage_ctx, offset, data, length);
}

xRETURN_t xBOOT_Storage_Erase(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, uint32_t length)
{
    if (ops == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ops->erase == NULL)
    {
        return xRETURN_xERR_xBOOT_UNSUPPORTED;
    }

    return ops->erase(storage_ctx, offset, length);
}

xRETURN_t xBOOT_Storage_Flush(const xBOOT_Storage_Ops_t *ops, void *storage_ctx)
{
    if (ops == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (ops->flush == NULL)
    {
        return xRETURN_xBOOT_OK; // Flush is optional, return success if NULL
    }

    return ops->flush(storage_ctx);
}

// EOF /////////////////////////////////////////////////////////////////////////////
