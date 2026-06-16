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

#ifndef XUSB_DFU_DEFS_H
#define XUSB_DFU_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "xusb_defs.h"

#define USB_CLASS_APPLICATION_SPECIFIC 0xFEU
#define USB_DFU_SUBCLASS               0x01U
#define USB_DFU_PROTOCOL_RUNTIME       0x01U // device is running application
#define USB_DFU_PROTOCOL_DFU           0x02U // device is in DFU mode
#define USB_DFU_FUNC_DESC_LEN          0x09U // DFU functional descriptor (DFU 1.1)
#define USB_DFU_FUNC_DESC_TYPE         0x21U // DFU_FUNCTIONAL

#define USB_DFU_ATTR_CAN_DNLOAD        (1U << 0)
#define USB_DFU_ATTR_CAN_UPLOAD        (1U << 1)
#define USB_DFU_ATTR_MANIFEST_TOLERANT (1U << 2) // device survives bus reset after manifest
#define USB_DFU_ATTR_WILL_DETACH       (1U << 3) // device self-resets after DFU_DETACH

#define USB_DFU_REQ_DETACH    0x00U
#define USB_DFU_REQ_DNLOAD    0x01U
#define USB_DFU_REQ_UPLOAD    0x02U
#define USB_DFU_REQ_GETSTATUS 0x03U
#define USB_DFU_REQ_CLRSTATUS 0x04U
#define USB_DFU_REQ_GETSTATE  0x05U
#define USB_DFU_REQ_ABORT     0x06U

#pragma pack(push, 1)

    typedef struct __attribute__((packed)) USB_DFU_Functional_Descriptor_t
    {
        uint8_t bLength;         // USB_DFU_FUNC_DESC_LEN (9)
        uint8_t bDescriptorType; // USB_DFU_FUNC_DESC_TYPE (0x21)
        uint8_t bmAttributes;
        uint16_t wDetachTimeout;
        uint16_t wTransferSize;
        uint16_t bcdDFUVersion;
    } USB_DFU_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_DFU_Descriptor_t
    {
        USB_Interface_Descriptor_t interface_descriptor;
        USB_DFU_Functional_Descriptor_t dfu_functional_descriptor;
    } USB_DFU_Descriptor_t;

    typedef struct __attribute__((packed)) USB_DFU_Status_Response_t
    {
        uint8_t bStatus;
        uint8_t bwPollTimeout[3]; // little-endian milliseconds
        uint8_t bState;
        uint8_t iString; // 0 = no string
    } USB_DFU_Status_Response_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_DFU_DEFS_H
