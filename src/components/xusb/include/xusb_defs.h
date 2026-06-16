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

#ifndef XUSB_DEFS_H
#define XUSB_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "xbytes.h"

// MACROS //////////////////////////////////////////////////////////////////////

// Module version
#define xUSB_VERSION_MAJOR  0U
#define xUSB_VERSION_MINOR  2U
#define xUSB_VERSION_PATCH  0U
#define xUSB_VERSION_STRING "0.2.0"

/* USB descriptor length */
#define USB_DEVICE_QUALIFIER_DESC_LEN      0x0AU
#define USB_DEVICE_DESC_LEN                0x12U
#define USB_CONFIGURATION_DESC_LEN         0x09U
#define USB_INTERFACE_DESC_LEN             0x09U
#define USB_ENDPOINT_DESC_LEN              0x07U
#define USB_SS_ENDPOINT_COMPANION_DESC_LEN 0x06U
#define USB_STRING_DESC_LEN                0x04U
#define USB_IAD_DESC_LEN                   0x08U

/* USB PID */
#define USB_PID_SOF   0x05U
#define USB_PID_SETUP 0x0DU
#define USB_PID_IN    0x09U
#define USB_PID_OUT   0x01U
#define USB_PID_ACK   0x02U
#define USB_PID_NAK   0x0AU
#define USB_PID_STALL 0x0EU
#define USB_PID_DATA0 0x03U
#define USB_PID_DATA1 0x0BU
#define USB_PID_PRE   0x0CU

/* USB descriptor type */
#define USB_DESC_TYPE_DEVICE                0x01U
#define USB_DESC_TYPE_CONFIGURATION         0x02U
#define USB_DESC_TYPE_STRING                0x03U
#define USB_DESC_TYPE_INTERFACE             0x04U
#define USB_DESC_TYPE_ENDPOINT              0x05U
#define USB_DESC_TYPE_QUALIFIER             0x06U
#define USB_DESC_TYPE_OTHER_SPEED           0x07U
#define USB_DESC_TYPE_OTG                   0x09U
#define USB_DESC_TYPE_HID                   0x21U
#define USB_DESC_TYPE_REPORT                0x22U
#define USB_DESC_TYPE_IAD                   0x0BU
#define USB_DESC_TYPE_BOS                   0X0FU
#define USB_DESC_TYPE_DEVICE_CAPABILITY     0x10U
#define USB_DESC_TYPE_SS_ENDPOINT_COMPANION 0x30U
#define USB_DESC_TYPE_CS_INTERFACE          0x24U
#define USB_DESC_TYPE_CS_ENDPOINT           0x25U

/* USB device class */
#define USB_CLASS_AUDIO         0x01U
#define USB_CLASS_COMMUNICATION 0x02U
#define USB_CLASS_HID           0x03U
#define USB_CLASS_PRINTER       0x07U
#define USB_CLASS_STORAGE       0x08U
#define USB_CLASS_HUB           0x09U
#define USB_CLASS_VIDEO         0x0EU
#define USB_CLASS_VENDOR        0xFFU

/* IAD device-level class codes - set in the device descriptor when any
 * configuration uses Interface Association Descriptors (IAD). */
#define USB_CLASS_IAD_DEVICE    0xEFU // Miscellaneous Device Class
#define USB_IAD_DEVICE_SUBCLASS 0x02U // Common Class
#define USB_IAD_DEVICE_PROTOCOL 0x01U // Interface Association Descriptor

/* USB endpoint type and attributes */
#define USB_ENDP_DIR_MASK  0x80U
#define USB_ENDP_ADDR_MASK 0x0FU
#define USB_ENDP_TYPE_MASK 0x03U
#define USB_ENDP_TYPE_CTRL 0x00U
#define USB_ENDP_TYPE_ISOC 0x01U
#define USB_ENDP_TYPE_BULK 0x02U
#define USB_ENDP_TYPE_INTR 0x03U

/* USB standard device request code */
#define USB_REQ_GET_STATUS        0x00U
#define USB_REQ_CLEAR_FEATURE     0x01U
#define USB_REQ_SET_FEATURE       0x03U
#define USB_REQ_SET_ADDRESS       0x05U
#define USB_REQ_GET_DESCRIPTOR    0x06U
#define USB_REQ_SET_DESCRIPTOR    0x07U
#define USB_REQ_GET_CONFIGURATION 0x08U
#define USB_REQ_SET_CONFIGURATION 0x09U
#define USB_REQ_GET_INTERFACE     0x0AU
#define USB_REQ_SET_INTERFACE     0x0BU
#define USB_REQ_SYNCH_FRAME       0x0CU
#define USB_REQ_SET_SEL           0x30U
#define USB_REQ_SET_ISOCH_DELAY   0x31U

/* Bit define for USB request type */
#define USB_REQ_TYPE_IN             0x80U
#define USB_REQ_TYPE_OUT            0x00U
#define USB_REQ_TYPE_MASK           0x60U
#define USB_REQ_TYPE_STANDARD       0x00U
#define USB_REQ_TYPE_CLASS          0x20U
#define USB_REQ_TYPE_VENDOR         0x40U
#define USB_REQ_RECIPIENT_MASK      0x1FU
#define USB_REQ_RECIPIENT_DEVICE    0x00U
#define USB_REQ_RECIPIENT_INTERFACE 0x01U
#define USB_REQ_RECIPIENT_ENDPOINT  0x02U

#define USB_CAPABILITY_WIRELESS_USB    0x01U
#define USB_CAPABILITY_20_EXTENTION    0x02U
#define USB_CAPABILITY_SUPER_SPEED_USB 0x03U
#define USB_CAPABILITY_CONTAINER_ID    0x04U

    typedef enum USB_Speed_t
    {
        USB_SPEED_LOW = 0x00,
        USB_SPEED_FULL = 0x01,
        USB_SPEED_HIGH = 0x02,
        USB_SPEED_SUPER = 0x03,
    } USB_Speed_t;

#pragma pack(push, 1)

    typedef struct __attribute__((packed)) USB_Setup_Request_t
    {
        uint8_t bRequestType;
        uint8_t bRequest;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
    } USB_Setup_Request_t;

    typedef struct __attribute__((packed)) USB_Device_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t bcdUSB;
        uint8_t bDeviceClass;
        uint8_t bDeviceSubClass;
        uint8_t bDeviceProtocol;
        uint8_t bMaxPacketSize0;
        uint16_t idVendor;
        uint16_t idProduct;
        uint16_t bcdDevice;
        uint8_t iManufacturer;
        uint8_t iProduct;
        uint8_t iSerialNumber;
        uint8_t bNumConfigurations;
    } USB_Device_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Device_Qualifier_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t bcdUSB;
        uint8_t bDeviceClass;
        uint8_t bDeviceSubClass;
        uint8_t bDeviceProtocol;
        uint8_t bMaxPacketSize0;
        uint8_t bNumConfigurations;
        uint8_t bReserved;
    } USB_Device_Qualifier_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Configuration_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t wTotalLength;
        uint8_t bNumInterfaces;
        uint8_t bConfigurationValue;
        uint8_t iConfiguration;
        uint8_t bmAttributes;
        uint8_t MaxPower;
    } USB_Configuration_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Interface_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bInterfaceNumber;
        uint8_t bAlternateSetting;
        uint8_t bNumEndpoints;
        uint8_t bInterfaceClass;
        uint8_t bInterfaceSubClass;
        uint8_t bInterfaceProtocol;
        uint8_t iInterface;
    } USB_Interface_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Endpoint_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bEndpointAddress;
        uint8_t bmAttributes;
        uint16_t wMaxPacketSize;
        uint8_t bInterval;
    } USB_Endpoint_Descriptor_t;

    typedef struct __attribute__((packed)) USB_SS_Endpoint_Companion_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bMaxBurst;
        uint8_t bmAttributes;
        uint16_t wBytesPerInterval;
    } USB_SS_Endpoint_Companion_Descriptor_t;

    typedef struct __attribute__((packed)) USB_BOS_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint16_t wTotalLength;
        uint8_t bNumDeviceCaps;
    } USB_BOS_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Device_Capability_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDevCapabilityType;
        uint32_t bmAttributes;
    } USB_Device_Capability_Descriptor_t;

    typedef struct __attribute__((packed)) USB_SS_Device_Capability_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bDevCapabilityType;
        uint8_t bmAttributes;
        uint16_t wSpeedsSupported;
        uint8_t bFunctionalitySupport;
        uint8_t bU1DevExitLat;
        uint16_t wU2DevExitLat;
    } USB_SS_Device_Capability_Descriptor_t;

    typedef struct __attribute__((packed)) USB_String_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
    } USB_String_Descriptor_t;

    typedef struct __attribute__((packed)) USB_Interface_Association_Descriptor_t
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bFirstInterface;
        uint8_t bInterfaceCount;
        uint8_t bFunctionClass;
        uint8_t bFunctionSubClass;
        uint8_t bFunctionProtocol;
        uint8_t iFunction;
    } USB_Interface_Association_Descriptor_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // XUSB_DEFS_H
