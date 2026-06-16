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

// @file xboot_image.c
// @brief Image header parsing and validation implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xboot_image.h"
#include "xboot_verify.h"

// DEBUG

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xBOOT_Image_Header_Parse(const uint8_t *buffer, uint32_t buffer_size, xBOOT_Image_Header_t *out_header)
{
    if ((buffer == NULL) || (out_header == NULL))
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (buffer_size < sizeof(xBOOT_Image_Header_t))
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    // Safely extract magic, version, size
    uint32_t magic;
    uint32_t header_version;
    uint32_t header_size;
    (void)memcpy(&magic, &buffer[offsetof(xBOOT_Image_Header_t, magic)], sizeof(uint32_t));
    (void)memcpy(&header_version, &buffer[offsetof(xBOOT_Image_Header_t, header_version)], sizeof(uint32_t));
    (void)memcpy(&header_size, &buffer[offsetof(xBOOT_Image_Header_t, header_size)], sizeof(uint32_t));

    // Validate headers
    if (magic != xBOOT_IMAGE_MAGIC)
    {
        return xRETURN_xERR_xBOOT_INVALID_IMAGE;
    }

    if (header_version != xBOOT_IMAGE_HEADER_VERSION_1)
    {
        return xRETURN_xERR_xBOOT_INVALID_IMAGE;
    }

    if (header_size != sizeof(xBOOT_Image_Header_t))
    {
        return xRETURN_xERR_xBOOT_INVALID_IMAGE;
    }

    // Validate header checksum
    uint32_t expected_crc;
    (void)memcpy(&expected_crc, &buffer[offsetof(xBOOT_Image_Header_t, header_crc32)], sizeof(uint32_t));
    uint32_t computed_crc = xBOOT_CRC32_Calculate(buffer, offsetof(xBOOT_Image_Header_t, header_crc32), xBOOT_CRC32_INIT) ^ 0xFFFFFFFFU;
    if (computed_crc != expected_crc)
    {
        return xRETURN_xERR_xBOOT_CRC_MISMATCH;
    }

    // Copy to output structure
    (void)memcpy(out_header, buffer, sizeof(xBOOT_Image_Header_t));

    return xRETURN_xBOOT_OK;
}

xRETURN_t xBOOT_Image_Validate_Bounds(const xBOOT_Image_Header_t *header, uint32_t mem_start, uint32_t mem_size)
{
    if (header == NULL)
    {
        return xRETURN_xERR_xBOOT_NULL_POINTER;
    }

    if (mem_size == 0U)
    {
        return xRETURN_xERR_xBOOT_INVALID_ARGUMENT;
    }

    // Boundary check for load address
    if ((header->load_address < mem_start) || (header->image_size > mem_size) ||
        ((header->load_address + header->image_size) < header->load_address) ||
        ((header->load_address + header->image_size) > (mem_start + mem_size)))
    {
        return xRETURN_xERR_xBOOT_INVALID_IMAGE;
    }

    // Boundary check for entry point
    if ((header->entry_address < header->load_address) || (header->entry_address >= (header->load_address + header->image_size)))
    {
        return xRETURN_xERR_xBOOT_INVALID_IMAGE;
    }

    return xRETURN_xBOOT_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
