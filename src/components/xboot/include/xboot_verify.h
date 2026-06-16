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

// @file xboot_verify.h
// @brief CRC32 verification utilities interface.
//

#ifndef XBOOT_VERIFY_H
#define XBOOT_VERIFY_H

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
#define xBOOT_CRC32_INIT 0xFFFFFFFFU

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Computes standard IEEE 802.3 CRC32 over the data.
     * @param data Pointer to input buffer
     * @param length Number of bytes to process
     * @param initial_crc Initial seed value (use xBOOT_CRC32_INIT for first block)
     * @return CRC32 checksum (post-XORed with 0xFFFFFFFF)
     */
    uint32_t xBOOT_CRC32_Calculate(const uint8_t *data, uint32_t length, uint32_t initial_crc);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_VERIFY_H
// EOF /////////////////////////////////////////////////////////////////////////////
