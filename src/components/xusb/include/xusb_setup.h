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

// @file xusb_setup.h
// @brief Bounds-checked USB setup-packet byte-buffer helpers.

#ifndef XUSB_SETUP_H
#define XUSB_SETUP_H

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
#include "xusb_defs.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xUSB_SETUP_REQUEST_SIZE                  8U
#define xUSB_SETUP_REQUEST_TYPE_OFFSET           0U
#define xUSB_SETUP_REQUEST_OFFSET                1U
#define xUSB_SETUP_REQUEST_VALUE_OFFSET          2U
#define xUSB_SETUP_REQUEST_INDEX_OFFSET          4U
#define xUSB_SETUP_REQUEST_LENGTH_OFFSET         6U
#define xUSB_SETUP_REQUEST_RECIPIENT_TYPE_MASK   0x1FU
#define xUSB_SETUP_REQUEST_DESCRIPTOR_TYPE_SHIFT 8U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline uint16_t xUSB_Setup_Get_Value(const USB_Setup_Request_t *request)
    {
        return xLE16_TO_CPU(request->wValue);
    }

    static inline uint16_t xUSB_Setup_Get_Index(const USB_Setup_Request_t *request)
    {
        return xLE16_TO_CPU(request->wIndex);
    }

    static inline uint16_t xUSB_Setup_Get_Length(const USB_Setup_Request_t *request)
    {
        return xLE16_TO_CPU(request->wLength);
    }

    static inline uint8_t xUSB_Setup_Get_Descriptor_Type(const USB_Setup_Request_t *request)
    {
        return (uint8_t)((xUSB_Setup_Get_Value(request) >> xUSB_SETUP_REQUEST_DESCRIPTOR_TYPE_SHIFT) & 0xFFU);
    }

    static inline uint8_t xUSB_Setup_Get_Descriptor_Index(const USB_Setup_Request_t *request)
    {
        return (uint8_t)(xUSB_Setup_Get_Value(request) & 0xFFU);
    }

    static inline void xUSB_Setup_Set_Value(USB_Setup_Request_t *request, uint16_t value)
    {
        request->wValue = xCPU_TO_LE16(value);
    }

    static inline void xUSB_Setup_Set_Index(USB_Setup_Request_t *request, uint16_t index)
    {
        request->wIndex = xCPU_TO_LE16(index);
    }

    static inline void xUSB_Setup_Set_Length(USB_Setup_Request_t *request, uint16_t length)
    {
        request->wLength = xCPU_TO_LE16(length);
    }

    static inline bool xUSB_Setup_Read(USB_Setup_Request_t *request, const uint8_t *buffer, uint32_t buffer_length)
    {
        if ((request == NULL) || (buffer == NULL) || (buffer_length < xUSB_SETUP_REQUEST_SIZE))
        {
            return false;
        }

        request->bRequestType = buffer[xUSB_SETUP_REQUEST_TYPE_OFFSET];
        request->bRequest = buffer[xUSB_SETUP_REQUEST_OFFSET];
        xUSB_Setup_Set_Value(request, xRead_LE16(&buffer[xUSB_SETUP_REQUEST_VALUE_OFFSET]));
        xUSB_Setup_Set_Index(request, xRead_LE16(&buffer[xUSB_SETUP_REQUEST_INDEX_OFFSET]));
        xUSB_Setup_Set_Length(request, xRead_LE16(&buffer[xUSB_SETUP_REQUEST_LENGTH_OFFSET]));
        return true;
    }

    static inline bool xUSB_Setup_Write(uint8_t *buffer, uint32_t buffer_length, const USB_Setup_Request_t *request)
    {
        if ((buffer == NULL) || (request == NULL) || (buffer_length < xUSB_SETUP_REQUEST_SIZE))
        {
            return false;
        }

        buffer[xUSB_SETUP_REQUEST_TYPE_OFFSET] = request->bRequestType;
        buffer[xUSB_SETUP_REQUEST_OFFSET] = request->bRequest;
        xWrite_LE16(&buffer[xUSB_SETUP_REQUEST_VALUE_OFFSET], xUSB_Setup_Get_Value(request));
        xWrite_LE16(&buffer[xUSB_SETUP_REQUEST_INDEX_OFFSET], xUSB_Setup_Get_Index(request));
        xWrite_LE16(&buffer[xUSB_SETUP_REQUEST_LENGTH_OFFSET], xUSB_Setup_Get_Length(request));
        return true;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSB_SETUP_H
// EOF /////////////////////////////////////////////////////////////////////////////
