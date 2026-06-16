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

#ifndef XUSB_CDC_DEFS_H
#define XUSB_CDC_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* CDC Class Subclass Code  */
#define USB_CDC_RESERVED                          0x00U
#define USB_CDC_DIRECT_LINE_CONTROL_MODEL         0x01U
#define USB_CDC_ABSTRACT_CONTROL_MODEL            0x02U
#define USB_CDC_TELEPHONE_CONTROL_MODEL           0x03U
#define USB_CDC_MULTI_CHANNEL_CONTROL_MODEL       0x04U
#define USB_CDC_CAPI_CONTROL_MODEL                0x05U
#define USB_CDC_ETHERNET_NETWORKING_CONTROL_MODEL 0x06U
#define USB_CDC_ATM_NETWORKING_CONTROL_MODEL      0x07U
#define USB_CDC_WIRELESS_HANDSET_CONTROL_MODEL    0x08U
#define USB_CDC_DEVICE_MANAGEMENT                 0x09U
#define USB_CDC_MOBILE_DIRECT_LINE_MODEL          0x0AU

/* CDC Functional Descriptor Lengths */
#define USB_CDC_HEADER_FUNC_DESC_LEN    0x05U
#define USB_CDC_CALL_MGMT_FUNC_DESC_LEN 0x05U
#define USB_CDC_ACM_FUNC_DESC_LEN       0x04U
#define USB_CDC_UNION_FUNC_DESC_LEN     0x05U

/* CDC class-specific request codes */
#define USB_CDC_SEND_ENCAPSULATED_COMMAND 0x00U
#define USB_CDC_GET_ENCAPSULATED_RESPONSE 0x01U
#define USB_CDC_SET_COMM_FEATURE          0x02U
#define USB_CDC_GET_COMM_FEATURE          0x03U
#define USB_CDC_CLEAR_COMM_FEATURE        0x04U
#define USB_CDC_SET_LINE_CODING           0x20U
#define USB_CDC_GET_LINE_CODING           0x21U
#define USB_CDC_SET_CONTROL_LINE_STATE    0x22U
#define USB_CDC_SEND_BREAK                0x23U

#pragma pack(push, 1)

    typedef struct __attribute__((packed)) USB_CDC_Header_Functional_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint16_t bcdCDC;
    } USB_CDC_Header_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_Call_Management_Functional_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bmCapabilities;
        uint8_t bDataInterface;
    } USB_CDC_Call_Management_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_Abstract_Control_Management_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bmCapabilities;
    } USB_CDC_Abstract_Control_Management_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_Union_Functional_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t bMasterInterface;
        uint8_t bSlaveInterface0;
    } USB_CDC_Union_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_Line_Code_t
    {
        uint32_t baud_rate;
        uint8_t stop_bits;
        uint8_t parity;
        uint8_t data_bits;
    } USB_CDC_Line_Code_t;

    typedef struct __attribute__((packed)) USB_CDC_Ethernet_Functional_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint8_t iMACAddress;
        uint32_t bmEthernetStatistics;
        uint16_t wMaxSegmentSize;
        uint16_t wNumberMCFilters;
        uint8_t bNumberPowerFilters;
    } USB_CDC_Ethernet_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_NCM_Functional_Descriptor_t
    {
        uint8_t bFunctionLength;
        uint8_t bDescriptorType;
        uint8_t bDescriptorSubtype;
        uint16_t bcdNcmVersion;
        uint8_t bmNetworkCapabilities;
    } USB_CDC_NCM_Functional_Descriptor_t;

    typedef struct __attribute__((packed)) USB_CDC_NCM_NTB_Parameters_t
    {
        uint16_t wLength;
        uint16_t bmNtbFormatsSupported;
        uint32_t dwNtbInMaxSize;
        uint16_t wNdpInDivisor;
        uint16_t wNdpInPayloadRemainder;
        uint16_t wNdpInAlignment;
        uint16_t wReserved;
        uint32_t dwNtbOutMaxSize;
        uint16_t wNdpOutDivisor;
        uint16_t wNdpOutPayloadRemainder;
        uint16_t wNdpOutAlignment;
        uint16_t wNtbOutMaxDatagrams;
    } USB_CDC_NCM_NTB_Parameters_t;

    typedef struct __attribute__((packed)) USB_CDC_Notification_Header_t
    {
        uint8_t bmRequestType;
        uint8_t bNotification;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
    } USB_CDC_Notification_Header_t;

#pragma pack(pop)

/* CDC ECM class-specific bRequest codes */
#define USB_CDC_ECM_SET_ETHERNET_MULTICAST_FILTERS 0x40U
#define USB_CDC_ECM_GET_ETHERNET_STATISTIC         0x42U
#define USB_CDC_ECM_SET_ETHERNET_PACKET_FILTER     0x43U

/* CDC NCM class-specific bRequest codes */
#define USB_CDC_NCM_GET_NTB_PARAMETERS    0x83U
#define USB_CDC_NCM_GET_NTB_FORMAT        0x84U
#define USB_CDC_NCM_SET_NTB_FORMAT        0x85U
#define USB_CDC_NCM_SET_NTB_INPUT_SIZE    0x87U
#define USB_CDC_NCM_SET_MAX_DATAGRAM_SIZE 0x89U

/* CDC class-specific notification bNotification codes */
#define USB_CDC_NOTIF_NETWORK_CONNECTION 0x00U
#define USB_CDC_NOTIF_RESPONSE_AVAILABLE 0x01U
#define USB_CDC_NOTIF_SPEED_CHANGE       0x2AU

/* Functional descriptor length constants */
#define USB_CDC_ETHERNET_FUNC_DESC_LEN 13U
#define USB_CDC_NCM_FUNC_DESC_LEN      6U

#ifdef __cplusplus
}
#endif

#endif // XUSB_CDC_DEFS_H
