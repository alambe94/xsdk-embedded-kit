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

// @file xboot_image.h
// @brief Image header parsing and validation interface.
//

#ifndef XBOOT_IMAGE_H
#define XBOOT_IMAGE_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_return.h"
#include "xboot_defs.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xBOOT_IMAGE_HEADER_VERSION_1 1U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xBOOT_Image_Header_t
    {
        uint32_t magic;          // xBOOT_IMAGE_MAGIC (0x58424F54U)
        uint32_t header_version; // Header structure version (e.g. 1)
        uint32_t header_size;    // Size of this header in bytes
        uint32_t image_size;     // Size of the payload image in bytes (excluding header)
        uint32_t load_address;   // Destination target execution address in memory
        uint32_t entry_address;  // Target CPU entry point execution address
        uint32_t version_major;  // Semantic version major
        uint32_t version_minor;  // Semantic version minor
        uint32_t version_patch;  // Semantic version patch
        uint32_t build_number;   // Build identifier
        uint32_t flags;          // Image flags
        uint32_t payload_crc32;  // CRC32 checksum of the payload data
        uint32_t header_crc32;   // CRC32 checksum of preceding header fields
    } xBOOT_Image_Header_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Parse and validate the image header from raw buffer.
     * @param buffer Raw buffer containing the image header
     * @param buffer_size Size of the raw buffer in bytes
     * @param out_header Pointer to output structure
     * @return xRETURN_t xRETURN_xBOOT_OK on success, error code otherwise
     */
    xRETURN_t xBOOT_Image_Header_Parse(const uint8_t *buffer, uint32_t buffer_size, xBOOT_Image_Header_t *out_header);

    /**
     * @brief Validates the image load and entry boundaries against memory limits.
     * @param header Parsed image header
     * @param mem_start Start execution address of target memory boundary
     * @param mem_size Total size of target memory boundary in bytes
     * @return xRETURN_t xRETURN_xBOOT_OK on success, error code otherwise
     */
    xRETURN_t xBOOT_Image_Validate_Bounds(const xBOOT_Image_Header_t *header, uint32_t mem_start, uint32_t mem_size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBOOT_IMAGE_H
// EOF /////////////////////////////////////////////////////////////////////////////
