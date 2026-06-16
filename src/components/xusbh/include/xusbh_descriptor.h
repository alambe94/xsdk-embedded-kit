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

// @file xusbh_descriptor.h
// @brief Host-side USB descriptor parsing and standard setup-packet helpers.

#ifndef XUSBH_DESCRIPTOR_H
#define XUSBH_DESCRIPTOR_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbh_return.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBH_Descriptor_Header_t
    {
        const uint8_t *data;
        uint32_t offset;
        uint8_t length;
        uint8_t type;
    } xUSBH_Descriptor_Header_t;

    typedef struct xUSBH_Descriptor_Walker_t
    {
        const uint8_t *buffer;
        uint32_t length;
        uint32_t offset;
    } xUSBH_Descriptor_Walker_t;

    typedef struct xUSBH_Device_Descriptor_t
    {
        uint16_t bcd_usb;
        uint8_t device_class;
        uint8_t device_subclass;
        uint8_t device_protocol;
        uint8_t ep0_max_packet_size;
        uint16_t vendor_id;
        uint16_t product_id;
        uint16_t bcd_device;
        uint8_t manufacturer_string_index;
        uint8_t product_string_index;
        uint8_t serial_string_index;
        uint8_t configuration_count;
    } xUSBH_Device_Descriptor_t;

    typedef struct xUSBH_Configuration_Descriptor_t
    {
        uint16_t total_length;
        uint8_t interface_count;
        uint8_t configuration_value;
        uint8_t configuration_string_index;
        uint8_t attributes;
        uint8_t max_power;
    } xUSBH_Configuration_Descriptor_t;

    typedef struct xUSBH_Interface_Descriptor_t
    {
        uint8_t interface_number;
        uint8_t alternate_setting;
        uint8_t endpoint_count;
        uint8_t class_code;
        uint8_t subclass;
        uint8_t protocol;
        uint8_t interface_string_index;
    } xUSBH_Interface_Descriptor_t;

    typedef struct xUSBH_Endpoint_Descriptor_t
    {
        uint8_t endpoint_address;
        uint8_t attributes;
        uint8_t endpoint_type;
        uint16_t max_packet_size;
        uint8_t interval;
    } xUSBH_Endpoint_Descriptor_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xUSBH_Device_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Device_Descriptor_t *descriptor);
    xRETURN_t
    xUSBH_Configuration_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Configuration_Descriptor_t *descriptor);
    xRETURN_t xUSBH_Interface_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Interface_Descriptor_t *descriptor);
    xRETURN_t xUSBH_Endpoint_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Endpoint_Descriptor_t *descriptor);
    xRETURN_t xUSBH_Configuration_Descriptor_Validate(const uint8_t *buffer, uint32_t buffer_length, uint16_t *total_length);
    xRETURN_t xUSBH_Descriptor_Walker_Init(xUSBH_Descriptor_Walker_t *walker, const uint8_t *buffer, uint32_t buffer_length);
    xRETURN_t xUSBH_Descriptor_Walker_Next(xUSBH_Descriptor_Walker_t *walker, xUSBH_Descriptor_Header_t *descriptor, bool *has_descriptor);
    xRETURN_t xUSBH_Setup_Build_Get_Descriptor(USB_Setup_Request_t *request,
                                               uint8_t descriptor_type,
                                               uint8_t descriptor_index,
                                               uint16_t index,
                                               uint16_t length);
    xRETURN_t xUSBH_Setup_Build_Set_Address(USB_Setup_Request_t *request, uint8_t address);
    xRETURN_t xUSBH_Setup_Build_Get_Configuration(USB_Setup_Request_t *request);
    xRETURN_t xUSBH_Setup_Build_Set_Configuration(USB_Setup_Request_t *request, uint8_t configuration_value);
    xRETURN_t xUSBH_Setup_Build_Get_Status(USB_Setup_Request_t *request, uint8_t recipient, uint16_t index);
    xRETURN_t xUSBH_Setup_Build_Clear_Feature(USB_Setup_Request_t *request, uint8_t recipient, uint16_t feature, uint16_t index);
    xRETURN_t xUSBH_Setup_Build_Set_Feature(USB_Setup_Request_t *request, uint8_t recipient, uint16_t feature, uint16_t index);
    xRETURN_t xUSBH_Setup_Build_Set_Interface(USB_Setup_Request_t *request, uint8_t interface_number, uint8_t alternate_setting);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_DESCRIPTOR_H
// EOF /////////////////////////////////////////////////////////////////////////////
