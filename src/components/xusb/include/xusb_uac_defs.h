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

#ifndef XUSB_UAC_DEFS_H
#define XUSB_UAC_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* UAC Subclass Codes */
#define USB_UAC_SUBCLASS_AUDIOCONTROL   0x01U
#define USB_UAC_SUBCLASS_AUDIOSTREAMING 0x02U

/* UAC AC Interface Descriptor Subtypes */
#define USB_UAC_AC_HEADER          0x01U
#define USB_UAC_AC_INPUT_TERMINAL  0x02U
#define USB_UAC_AC_OUTPUT_TERMINAL 0x03U
#define USB_UAC_AC_FEATURE_UNIT    0x06U

/* UAC AS Interface Descriptor Subtypes */
#define USB_UAC_AS_HEADER      0x01U
#define USB_UAC_AS_FORMAT_TYPE 0x02U

/* UAC CS Endpoint Subtype */
#define USB_UAC_EP_GENERAL 0x01U

/* UAC Terminal Types (stored as two bytes L/H in descriptor) */
#define USB_UAC_TT_USB_STREAMING_L 0x01U /* 0x0101 */
#define USB_UAC_TT_USB_STREAMING_H 0x01U
#define USB_UAC_TT_SPEAKER_L       0x01U /* 0x0301 */
#define USB_UAC_TT_SPEAKER_H       0x03U
#define USB_UAC_TT_MICROPHONE_L    0x01U /* 0x0201 */
#define USB_UAC_TT_MICROPHONE_H    0x02U

/* UAC Channel Configuration bits (wChannelConfig) */
#define USB_UAC_CHAN_LEFT_FRONT  (1U << 0)
#define USB_UAC_CHAN_RIGHT_FRONT (1U << 1)

/* UAC Class-specific Request Codes */
#define USB_UAC_REQ_SET_CUR 0x01U
#define USB_UAC_REQ_GET_CUR 0x81U
#define USB_UAC_REQ_GET_MIN 0x82U
#define USB_UAC_REQ_GET_MAX 0x83U
#define USB_UAC_REQ_GET_RES 0x84U

/* UAC Feature Unit Control Selectors */
#define USB_UAC_FU_MUTE_CONTROL   0x01U
#define USB_UAC_FU_VOLUME_CONTROL 0x02U

/* UAC Format Type */
#define USB_UAC_FORMAT_TYPE_I    0x01U
#define USB_UAC_FORMAT_TAG_PCM_L 0x01U /* 0x0001 PCM */
#define USB_UAC_FORMAT_TAG_PCM_H 0x00U

/* Isochronous endpoint bmAttributes full values
 * bits[1:0] = transfer type (01=Iso)
 * bits[3:2] = sync type    (00=None, 01=Async, 10=Adaptive, 11=Sync)
 * bits[5:4] = usage type   (00=Data, 01=Feedback) */
#define USB_ENDP_ISOC_ASYNC_DATA    0x05U /* Iso+Async+Data    */
#define USB_ENDP_ISOC_ADAPTIVE_DATA 0x09U /* Iso+Adaptive+Data */
#define USB_ENDP_ISOC_SYNC_DATA     0x0DU /* Iso+Sync+Data     */
#define USB_ENDP_ISOC_FEEDBACK_EP   0x11U /* Iso+Feedback      */

/* Feedback endpoint refresh period: bRefresh=9 -> every 2^9=512 SOF frames */
#define USB_UAC_FEEDBACK_REFRESH 0x09U

/* UAC descriptor byte lengths */
#define USB_UAC_AC_HEADER_DESC_LEN           0x0AU /* 2 streaming interfaces */
#define USB_UAC_INPUT_TERMINAL_DESC_LEN      0x0CU
#define USB_UAC_OUTPUT_TERMINAL_DESC_LEN     0x09U
#define USB_UAC_FEATURE_UNIT_DESC_LEN        0x0AU /* 7 + 3*bControlSize(1) */
#define USB_UAC_AS_HEADER_DESC_LEN           0x07U
#define USB_UAC_FORMAT_TYPE_I_1FREQ_DESC_LEN 0x0BU /* 1 discrete frequency  */
#define USB_UAC_AS_ENDPOINT_DESC_LEN         0x07U
#define USB_UAC_ENDPOINT_DESC_LEN            0x09U /* 7 + bRefresh + bSynchAddress */

#pragma pack(push, 1)

    /* AC Interface Class-Specific Header (fixed for 2 streaming interfaces) */
    typedef struct __attribute__((packed)) USB_UAC_AC_Header_Descriptor_t
    {
        uint8_t bLength;            /* USB_UAC_AC_HEADER_DESC_LEN */
        uint8_t bDescriptorType;    /* USB_DESC_TYPE_CS_INTERFACE */
        uint8_t bDescriptorSubtype; /* USB_UAC_AC_HEADER */
        uint16_t bcdADC;            /* 0x0100 -> ADC 1.00 */
        uint16_t wTotalLength;      /* total AC block byte count */
        uint8_t bInCollection;      /* 2 = speaker + mic */
        uint8_t baInterfaceNr[2];   /* [speaker_itf, mic_itf] */
    } USB_UAC_AC_Header_Descriptor_t;

    typedef struct __attribute__((packed)) USB_UAC_Input_Terminal_Descriptor_t
    {
        uint8_t bLength;            /* USB_UAC_INPUT_TERMINAL_DESC_LEN */
        uint8_t bDescriptorType;    /* USB_DESC_TYPE_CS_INTERFACE */
        uint8_t bDescriptorSubtype; /* USB_UAC_AC_INPUT_TERMINAL */
        uint8_t bTerminalID;
        uint16_t wTerminalType;
        uint8_t bAssocTerminal; /* 0 = none */
        uint8_t bNrChannels;
        uint16_t wChannelConfig;
        uint8_t iChannelNames; /* 0 = none */
        uint8_t iTerminal;     /* 0 = none */
    } USB_UAC_Input_Terminal_Descriptor_t;

    typedef struct __attribute__((packed)) USB_UAC_Output_Terminal_Descriptor_t
    {
        uint8_t bLength; /* USB_UAC_OUTPUT_TERMINAL_DESC_LEN */
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype; /* USB_UAC_AC_OUTPUT_TERMINAL */
        uint8_t bTerminalID;
        uint16_t wTerminalType;
        uint8_t bAssocTerminal; /* 0 = none */
        uint8_t bSourceID;      /* upstream unit/terminal ID */
        uint8_t iTerminal;      /* 0 = none */
    } USB_UAC_Output_Terminal_Descriptor_t;

    /* Feature Unit - master channel only.
     * bmaControls[3]: index 0=master (Mute+Volume), 1=L (0x00), 2=R (0x00).
     * Host presents a single master volume slider; per-channel controls are off. */
    typedef struct __attribute__((packed)) USB_UAC_Feature_Unit_Descriptor_t
    {
        uint8_t bLength; /* USB_UAC_FEATURE_UNIT_DESC_LEN = 10 */
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype; /* USB_UAC_AC_FEATURE_UNIT */
        uint8_t bUnitID;
        uint8_t bSourceID;
        uint8_t bControlSize;   /* 1 byte per channel entry */
        uint8_t bmaControls[3]; /* [master, L, R] */
        uint8_t iFeature;       /* 0 = none */
    } USB_UAC_Feature_Unit_Descriptor_t;

    /* Class-Specific AS Interface Header */
    typedef struct __attribute__((packed)) USB_UAC_AS_Header_Descriptor_t
    {
        uint8_t bLength; /* USB_UAC_AS_HEADER_DESC_LEN */
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype; /* USB_UAC_AS_GENERAL */
        uint8_t bTerminalLink;      /* terminal ID this AS interface is connected to */
        uint8_t bDelay;             /* interface pipeline delay in frames */
        uint16_t wFormatTag;
    } USB_UAC_AS_Header_Descriptor_t;

    /* Type I Format - fixed to 1 discrete sample frequency */
    typedef struct __attribute__((packed)) USB_UAC_Format_Type_I_Descriptor_t
    {
        uint8_t bLength; /* USB_UAC_FORMAT_TYPE_I_1FREQ_DESC_LEN */
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype; /* USB_UAC_AS_FORMAT_TYPE */
        uint8_t bFormatType;        /* USB_UAC_FORMAT_TYPE_I */
        uint8_t bNrChannels;
        uint8_t bSubframeSize;  /* bytes per audio subframe (2 for 16-bit) */
        uint8_t bBitResolution; /* bits per sample (16) */
        uint8_t bSamFreqType;   /* 1 = one discrete frequency */
        uint8_t tSamFreq[3];    /* 3-byte little-endian sample rate (e.g. 48000 = 0x00BB80) */
    } USB_UAC_Format_Type_I_Descriptor_t;

    /* Class-Specific AS Endpoint Descriptor (follows the standard iso endpoint) */
    typedef struct __attribute__((packed)) USB_UAC_AS_Endpoint_Descriptor_t
    {
        uint8_t bLength;            /* USB_UAC_AS_ENDPOINT_DESC_LEN */
        uint8_t bDescriptorType;    /* USB_DESC_TYPE_CS_ENDPOINT */
        uint8_t bDescriptorSubtype; /* USB_UAC_EP_GENERAL */
        uint8_t bmAttributes;       /* 0x00 for fixed-rate (no pitch/freq control) */
        uint8_t bLockDelayUnits;    /* 0x00 */
        uint16_t wLockDelay;
    } USB_UAC_AS_Endpoint_Descriptor_t;

    /* Extended isochronous endpoint descriptor - 9 bytes (standard 7 + bRefresh + bSynchAddress).
     * Replaces USB_Endpoint_Descriptor_t for UAC audio data and feedback endpoints. */
    typedef struct __attribute__((packed)) USB_UAC_Endpoint_Descriptor_t
    {
        uint8_t bLength;         /* USB_UAC_ENDPOINT_DESC_LEN = 9 */
        uint8_t bDescriptorType; /* USB_DESC_TYPE_ENDPOINT */
        uint8_t bEndpointAddress;
        uint8_t bmAttributes; /* USB_ENDP_ISOC_*_DATA or USB_ENDP_ISOC_FEEDBACK_EP */
        uint16_t wMaxPacketSize;
        uint8_t bInterval;     /* 0x01 = every frame (1 ms for FS) */
        uint8_t bRefresh;      /* 0x00 for data EPs; USB_UAC_FEEDBACK_REFRESH for feedback */
        uint8_t bSynchAddress; /* feedback EP address for async data EP; 0x00 otherwise */
    } USB_UAC_Endpoint_Descriptor_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_UAC_DEFS_H
