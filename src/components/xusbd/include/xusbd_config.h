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

// @file xusbd_config.h
// @brief Global configuration macros for the xUSB Device Stack.

#ifndef XUSBD_CONFIG_H
#define XUSBD_CONFIG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////

// Module version
#define xUSBD_VERSION_MAJOR  0U
#define xUSBD_VERSION_MINOR  16U
#define xUSBD_VERSION_PATCH  0U
#define xUSBD_VERSION_STRING "0.16.0"

#ifndef xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE
#define xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE 1024U
#endif

#ifndef xUSBD_MAX_BOS_DESCRIPTOR_SIZE
#define xUSBD_MAX_BOS_DESCRIPTOR_SIZE 256U
#endif

#ifndef xUSBD_MAX_MOS2_DESCRIPTOR_SIZE
#define xUSBD_MAX_MOS2_DESCRIPTOR_SIZE 512U
#endif

#ifndef xUSBD_MAX_EP0_DATA_SIZE
#define xUSBD_MAX_EP0_DATA_SIZE 256U
#endif

#ifndef xUSBD_MAX_STRING_DESCRIPTOR_SIZE
#define xUSBD_MAX_STRING_DESCRIPTOR_SIZE 32U
#endif

#ifndef xUSBD_MAX_INTERFACE_COUNT
#define xUSBD_MAX_INTERFACE_COUNT 16U
#endif

#ifndef xUSBD_MAX_ENDPOINT_MAP_ENTRIES
#define xUSBD_MAX_ENDPOINT_MAP_ENTRIES 32U
#endif

#ifndef xUSBD_MAX_STRING_MAP_ENTRIES
#define xUSBD_MAX_STRING_MAP_ENTRIES 32U
#endif

// USB protocol string descriptor index assignments - not overridable.
#define xUSBD_LANG_ID_STRING_INDEX 0x00U
#define xUSBD_VENDOR_STRING_INDEX  0x01U
#define xUSBD_PRODUCT_STRING_INDEX 0x02U
#define xUSBD_SERIAL_STRING_INDEX  0x03U
#define xUSBD_MSOS_STRING_INDEX    0xEEU

#ifndef xUSBD_ENABLE_MOS2
#define xUSBD_ENABLE_MOS2 1U
#endif

#ifndef xUSBD_LANG_ID
#define xUSBD_LANG_ID 1033U
#endif

#ifndef xUSBD_WINUSB_VENDOR_CODE
#define xUSBD_WINUSB_VENDOR_CODE 0x94U
#endif

#ifndef xUSBD_CONFIG_LOG_LEVEL
#define xUSBD_CONFIG_LOG_LEVEL 0
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
#endif // XUSBD_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
