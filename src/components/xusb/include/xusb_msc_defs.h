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

#ifndef XUSB_MSC_DEFS_H
#define XUSB_MSC_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* MSC Subclass Code  */
#define USB_MSC_SCSI_COMMAND_SET_NOT_REPORTED 0x00U
#define USB_MSC_RBC                           0x01U
#define USB_MSC_MMC_5                         0x02U
#define USB_MSC_OBSOLETE                      0x03U
#define USB_MSC_UFI                           0x04U
#define USB_MSC_OBSOLETE2                     0x05U
#define USB_MSC_SCSI_TRANSPARENT_COMMAND_SET  0x06U
#define USB_MSC_LSD_FS                        0x07U
#define USB_MSC_IEEE_1667                     0x08U
#define USB_MSC_SPECIFIC_TO_DEVICE_VENDOR     0xFFU

/* MSC Protocol Code  */
#define USB_MSC_CBI_INTERRUPT    0x00U
#define USB_MSC_CBI_NO_INTERRUPT 0x01U
#define USB_MSC_BBB              0x50U
#define USB_MSC_UAS              0x62U
#define USB_MSC_FEH              0x63U

/* MSC Class requests */
#define USB_MSC_ADSC             0x01U
#define USB_MSC_REQ_GET_REQUESTS 0xFCU
#define USB_MSC_REQ_PUT_REQUESTS 0xFDU
#define USB_MSC_REQ_GET_MAX_LUN  0xFEU
#define USB_MSC_REQ_SET_BOMSR    0xFFU

/* MSC BOT default */
#define USB_MSC_BOT_CBW_SIGNATURE 0x43425355U
#define USB_MSC_BOT_CSW_SIGNATURE 0x53425355U
#define USB_MSC_BOT_CBW_LENGTH    0x1FU
#define USB_MSC_BOT_CSW_LENGTH    0x0DU
#define USB_MSC_BOT_MAX_DATA      0x100U

/* MSC BOT SCSI Commands */
#define USB_MSC_TEST_UNIT_READY              0x00U
#define USB_MSC_REWIND                       0x01U
#define USB_MSC_REQUEST_SENSE                0x03U
#define USB_MSC_FORMAT                       0x04U
#define USB_MSC_INQUIRY                      0x12U
#define USB_MSC_MODE_SELECT6                 0x15U
#define USB_MSC_RELEASE6                     0x17U
#define USB_MSC_MODE_SENSE6                  0x1AU
#define USB_MSC_START_STOP_UNIT              0x1BU
#define USB_MSC_SEND_DIAGNOSTIC              0x1DU
#define USB_MSC_PREVENT_ALLOW_MEDIUM_REMOVAL 0x1EU
#define USB_MSC_READ_FORMAT_CAPACITIES       0x23U
#define USB_MSC_READ_CAPACITY                0x25U
#define USB_MSC_READ10                       0x28U
#define USB_MSC_WRITE10                      0x2AU
#define USB_MSC_SEEK10                       0x2BU
#define USB_MSC_WRITE_AND_VERIFY10           0x2EU
#define USB_MSC_VERIFY10                     0x2FU
#define USB_MSC_SYNCHRONIZE_CACHE            0x35U
#define USB_MSC_MODE_SENSE10                 0x5AU
#define USB_MSC_READ12                       0xA8U
#define USB_MSC_WRITE12                      0xAAU

#pragma pack(push, 1)

    // USB MSC BOT spec: CBW is 31 bytes; pad_ rounds to 32 for 4-byte DMA alignment.
    typedef struct __attribute__((packed)) USB_MSC_BOT_CBW_t
    {
        uint8_t dCBWSignature[4];
        uint8_t dCBWTag[4];
        uint8_t dCBWDataLength[4];
        uint8_t bmCBWFlags;
        uint8_t bCBWLUN;
        uint8_t bCBWCBLength;
        uint8_t CBWCB[16];
        uint8_t pad_; // DMA alignment - do not remove
    } USB_MSC_BOT_CBW_t;

    // USB MSC BOT spec: CSW is 13 bytes; pad_ rounds to 16 for 4-byte DMA alignment.
    typedef struct __attribute__((packed)) USB_MSC_BOT_CSW_t
    {
        uint8_t dCSWSignature[4];
        uint8_t dCSWTag[4];
        uint8_t dCSWDataResidue[4];
        uint8_t bCSWStatus;
        uint8_t pad_[3]; // DMA alignment - do not remove
    } USB_MSC_BOT_CSW_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_MSC_DEFS_H
