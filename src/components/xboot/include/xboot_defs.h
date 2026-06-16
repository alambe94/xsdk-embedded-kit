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

// @file xboot_defs.h
// @brief Common constants, limits, and alignment helpers for xBOOT.
//

#ifndef XBOOT_DEFS_H
#define XBOOT_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////
#define xBOOT_IMAGE_MAGIC     0x58424F54U // 'XBOT'
#define xBOOT_PARTITION_MAGIC 0x58425054U // 'XBPT'
#define xBOOT_INVALID_ADDRESS 0xFFFFFFFFU
#define xBOOT_INVALID_SLOT_ID 0xFFFFFFFFU
#define xBOOT_MAX_SLOTS       2U
#define xBOOT_MAX_PARTITIONS  8U

// Endianness and alignment helpers
#define xBOOT_ALIGN_DOWN(val, align) ((val) & ~((align) - 1U))
#define xBOOT_ALIGN_UP(val, align)   (((val) + ((align) - 1U)) & ~((align) - 1U))
#define xBOOT_IS_ALIGNED(val, align) (((val) & ((align) - 1U)) == 0U)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
