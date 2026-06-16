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

// @file xboot_storage.h
// @brief Storage operations interface and helpers for xBOOT.
//

#ifndef XBOOT_STORAGE_H
#define XBOOT_STORAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_defs.h"
#include "xboot_config.h"
#include "xboot_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct xBOOT_Storage_Ops_t
    {
        xRETURN_t (*read)(void *storage_ctx, uint32_t offset, uint8_t *data, uint32_t length);

        xRETURN_t (*write)(void *storage_ctx, uint32_t offset, const uint8_t *data, uint32_t length);

        xRETURN_t (*erase)(void *storage_ctx, uint32_t offset, uint32_t length);

        xRETURN_t (*flush)(void *storage_ctx);
    } xBOOT_Storage_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    xRETURN_t xBOOT_Storage_Read(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, uint8_t *data, uint32_t length);

    xRETURN_t xBOOT_Storage_Write(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, const uint8_t *data, uint32_t length);

    xRETURN_t xBOOT_Storage_Erase(const xBOOT_Storage_Ops_t *ops, void *storage_ctx, uint32_t offset, uint32_t length);

    xRETURN_t xBOOT_Storage_Flush(const xBOOT_Storage_Ops_t *ops, void *storage_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_STORAGE_H
// EOF /////////////////////////////////////////////////////////////////////////////
