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

// @file xusb_descriptor.h
// @brief Bounds-checked helpers for USB descriptor byte buffers.

#ifndef XUSB_DESCRIPTOR_H
#define XUSB_DESCRIPTOR_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xbytes.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xUSB_DESCRIPTOR_HEADER_SIZE   2U
#define xUSB_DESCRIPTOR_LENGTH_OFFSET 0U
#define xUSB_DESCRIPTOR_TYPE_OFFSET   1U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline bool xUSB_Descriptor_Read_U8(const uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint8_t *value)
    {
        if ((buffer == NULL) || (value == NULL) || (offset >= buffer_length))
        {
            return false;
        }

        *value = buffer[offset];
        return true;
    }

    static inline bool xUSB_Descriptor_Read_LE16(const uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint16_t *value)
    {
        if ((buffer == NULL) || (value == NULL) || (buffer_length < 2U) || (offset > (buffer_length - 2U)))
        {
            return false;
        }

        *value = xRead_LE16(&buffer[offset]);
        return true;
    }

    static inline bool xUSB_Descriptor_Read_LE32(const uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint32_t *value)
    {
        if ((buffer == NULL) || (value == NULL) || (buffer_length < 4U) || (offset > (buffer_length - 4U)))
        {
            return false;
        }

        *value = xRead_LE32(&buffer[offset]);
        return true;
    }

    static inline bool xUSB_Descriptor_Write_U8(uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint8_t value)
    {
        if ((buffer == NULL) || (offset >= buffer_length))
        {
            return false;
        }

        buffer[offset] = value;
        return true;
    }

    static inline bool xUSB_Descriptor_Write_LE16(uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint16_t value)
    {
        if ((buffer == NULL) || (buffer_length < 2U) || (offset > (buffer_length - 2U)))
        {
            return false;
        }

        xWrite_LE16(&buffer[offset], value);
        return true;
    }

    static inline bool xUSB_Descriptor_Write_LE32(uint8_t *buffer, uint32_t buffer_length, uint32_t offset, uint32_t value)
    {
        if ((buffer == NULL) || (buffer_length < 4U) || (offset > (buffer_length - 4U)))
        {
            return false;
        }

        xWrite_LE32(&buffer[offset], value);
        return true;
    }

    static inline bool
    xUSB_Descriptor_Read_Header(const uint8_t *buffer, uint32_t buffer_length, uint8_t *descriptor_length, uint8_t *descriptor_type)
    {
        if ((buffer == NULL) || (descriptor_length == NULL) || (descriptor_type == NULL) || (buffer_length < xUSB_DESCRIPTOR_HEADER_SIZE))
        {
            return false;
        }

        *descriptor_length = buffer[xUSB_DESCRIPTOR_LENGTH_OFFSET];
        *descriptor_type = buffer[xUSB_DESCRIPTOR_TYPE_OFFSET];
        return true;
    }

    static inline bool xUSB_Descriptor_Is_Complete(const uint8_t *buffer, uint32_t buffer_length)
    {
        uint8_t descriptor_length = 0U;
        uint8_t descriptor_type = 0U;

        if (!xUSB_Descriptor_Read_Header(buffer, buffer_length, &descriptor_length, &descriptor_type))
        {
            return false;
        }

        (void)descriptor_type;
        return (descriptor_length >= xUSB_DESCRIPTOR_HEADER_SIZE) && ((uint32_t)descriptor_length <= buffer_length);
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSB_DESCRIPTOR_H
// EOF /////////////////////////////////////////////////////////////////////////////
