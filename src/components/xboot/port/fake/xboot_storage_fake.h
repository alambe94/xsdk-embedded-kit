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

// @file xboot_storage_fake.h
// @brief Host-testable memory-backed fake flash storage simulation header.
//

#ifndef XBOOT_STORAGE_FAKE_H
#define XBOOT_STORAGE_FAKE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_storage.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef struct
    {
        uint8_t *buffer;
        uint32_t size;
        uint32_t sector_size;

        bool inject_read_fail;
        bool inject_write_fail;
        bool inject_erase_fail;
    } xBOOT_Storage_Fake_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    const xBOOT_Storage_Ops_t *xBOOT_Storage_Fake_Ops(void);

    xRETURN_t xBOOT_Storage_Fake_Init(xBOOT_Storage_Fake_Context_t *ctx, uint8_t *buffer, uint32_t size, uint32_t sector_size);

    void xBOOT_Storage_Fake_Reset_Errors(xBOOT_Storage_Fake_Context_t *ctx);
    void xBOOT_Storage_Fake_Inject_Read_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable);
    void xBOOT_Storage_Fake_Inject_Write_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable);
    void xBOOT_Storage_Fake_Inject_Erase_Error(xBOOT_Storage_Fake_Context_t *ctx, bool enable);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_STORAGE_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
