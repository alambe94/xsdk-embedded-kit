// Copyright 2022 alambe94
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

#ifndef XUSB_WIN_DEFS_H
#define XUSB_WIN_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define USB_MOS1_COMPAT_ID_DESC         0x04U
#define USB_MOS1_EXTENDED_PROPERTY_DESC 0x05U

#define USB_MOS2_SET_HEADER_DESCRIPTOR       0x0000U
#define USB_MOS2_SUBSET_HEADER_CONFIGURATION 0x0002U
#define USB_MOS2_SUBSET_HEADER_FUNCTION      0x0003U

#pragma pack(push, 1)

    typedef struct __attribute__((packed))
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDevCapabilityType;
        uint8_t bReserved;
        uint8_t PlatformCapabilityUUID[16];
        uint32_t dwWindowsVersion;
        uint16_t wMSOSDescriptorSetTotalLength;
        uint8_t bMS_VendorCode;
        uint8_t bAltEnumCode;
    } USB_MS_OS_20_Platform_Cap_t;

    typedef struct __attribute__((packed))
    {
        uint16_t wLength;
        uint16_t wDescriptorType;
        uint32_t dwWindowsVersion;
        uint16_t wTotalLength;
    } USB_MS_OS_20_Set_Header_Descriptor_t;

    typedef struct __attribute__((packed))
    {
        uint16_t wLength;
        uint16_t wDescriptorType;
        uint8_t bConfigurationValue;
        uint8_t bReserved;
        uint16_t wTotalLength;
    } USB_MS_OS_20_Subset_Header_Configuration_t;

    typedef struct __attribute__((packed))
    {
        uint16_t wLength;
        uint16_t wDescriptorType;
        uint8_t bFirstInterface;
        uint8_t bReserved;
        uint16_t wSubsetLength;
    } USB_MS_OS_20_Subset_Header_Function_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_WIN_DEFS_H
