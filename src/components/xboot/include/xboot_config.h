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

// @file xboot_config.h
// @brief Compile-time configuration limits and platform boundaries for xBOOT.
//

#ifndef XBOOT_CONFIG_H
#define XBOOT_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_defs.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xBOOT_CONFIG_MAX_PARTITIONS  xBOOT_MAX_PARTITIONS
#define xBOOT_CONFIG_MAX_IMAGE_SIZE  0x00780000U // Max image size: 7.5 MB
#define xBOOT_CONFIG_SECTOR_SIZE     65536U      // 64 KB sector erase size
#define xBOOT_CONFIG_MSRAM_APP_START 0x70040000U // Application start boundary in MSRAM
#define xBOOT_CONFIG_MSRAM_APP_END   0x700FFFFFU // Application end boundary in MSRAM

// Verification configuration
#define xBOOT_CONFIG_VERIFY_CRC32 1

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
