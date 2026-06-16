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

#ifndef XUSB_HID_DEFS_H
#define XUSB_HID_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* HID descriptor length */
#define USB_HID_DESC_LEN 0x09U

/* HID Class requests */
#define USB_HID_REQ_GET_REPORT   0x01U
#define USB_HID_REQ_GET_IDLE     0x02U
#define USB_HID_REQ_GET_PROTOCOL 0x03U
#define USB_HID_REQ_SET_REPORT   0x09U
#define USB_HID_REQ_SET_IDLE     0x0AU
#define USB_HID_REQ_SET_PROTOCOL 0x0BU

#pragma pack(push, 1)

    typedef struct __attribute__((packed)) USB_HID_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t bcdHID;
        uint8_t bCountryCode;
        uint8_t bNumDescriptors;
        uint8_t bDescriptorType1;
        uint16_t wItemLength;
    } USB_HID_Descriptor_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_HID_DEFS_H
