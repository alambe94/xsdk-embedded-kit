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

// @file xusbh_descriptor.c
// @brief Host-side USB descriptor parsing and setup-packet builders.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_descriptor.h"
#include "xusb_setup.h"
#include "xusbh_descriptor.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define DEVICE_DESC_BCD_USB_OFFSET      2U
#define DEVICE_DESC_CLASS_OFFSET        4U
#define DEVICE_DESC_SUBCLASS_OFFSET     5U
#define DEVICE_DESC_PROTOCOL_OFFSET     6U
#define DEVICE_DESC_EP0_MPS_OFFSET      7U
#define DEVICE_DESC_VENDOR_ID_OFFSET    8U
#define DEVICE_DESC_PRODUCT_ID_OFFSET   10U
#define DEVICE_DESC_BCD_DEVICE_OFFSET   12U
#define DEVICE_DESC_MANUFACTURER_OFFSET 14U
#define DEVICE_DESC_PRODUCT_OFFSET      15U
#define DEVICE_DESC_SERIAL_OFFSET       16U
#define DEVICE_DESC_CONFIG_COUNT_OFFSET 17U

#define CONFIG_DESC_TOTAL_LENGTH_OFFSET    2U
#define CONFIG_DESC_INTERFACE_COUNT_OFFSET 4U
#define CONFIG_DESC_VALUE_OFFSET           5U
#define CONFIG_DESC_STRING_OFFSET          6U
#define CONFIG_DESC_ATTRIBUTES_OFFSET      7U
#define CONFIG_DESC_MAX_POWER_OFFSET       8U

#define INTERFACE_DESC_NUMBER_OFFSET         2U
#define INTERFACE_DESC_ALT_SETTING_OFFSET    3U
#define INTERFACE_DESC_ENDPOINT_COUNT_OFFSET 4U
#define INTERFACE_DESC_CLASS_OFFSET          5U
#define INTERFACE_DESC_SUBCLASS_OFFSET       6U
#define INTERFACE_DESC_PROTOCOL_OFFSET       7U
#define INTERFACE_DESC_STRING_OFFSET         8U

#define ENDPOINT_DESC_ADDRESS_OFFSET          2U
#define ENDPOINT_DESC_ATTRIBUTES_OFFSET       3U
#define ENDPOINT_DESC_MAX_PACKET_SIZE_OFFSET  4U
#define ENDPOINT_DESC_INTERVAL_OFFSET         6U
#define USB_STANDARD_DEVICE_ADDRESS_MAX       127U
#define USB_SETUP_DESCRIPTOR_VALUE_TYPE_SHIFT 8U
#define USB_GET_STATUS_RESPONSE_SIZE          2U

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t descriptor_header_parse(const uint8_t *buffer,
                                         uint32_t buffer_length,
                                         uint8_t min_length,
                                         uint8_t expected_type,
                                         xUSBH_Descriptor_Header_t *header);
static xRETURN_t
descriptor_header_parse_at(const uint8_t *buffer, uint32_t buffer_length, uint32_t offset, xUSBH_Descriptor_Header_t *header);
static xRETURN_t setup_request_build(USB_Setup_Request_t *request,
                                     uint8_t request_type,
                                     uint8_t request_code,
                                     uint16_t value,
                                     uint16_t index,
                                     uint16_t length);
static xRETURN_t setup_recipient_validate(uint8_t recipient);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static xRETURN_t descriptor_header_parse(const uint8_t *buffer,
                                         uint32_t buffer_length,
                                         uint8_t min_length,
                                         uint8_t expected_type,
                                         xUSBH_Descriptor_Header_t *header)
{
    xRETURN_t status = descriptor_header_parse_at(buffer, buffer_length, 0U, header);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((header->type != expected_type) || (header->length < min_length))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

static xRETURN_t
descriptor_header_parse_at(const uint8_t *buffer, uint32_t buffer_length, uint32_t offset, xUSBH_Descriptor_Header_t *header)
{
    uint8_t descriptor_length = 0U;
    uint8_t descriptor_type = 0U;

    if ((buffer == NULL) || (header == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((buffer_length < xUSB_DESCRIPTOR_HEADER_SIZE) || (offset > (buffer_length - xUSB_DESCRIPTOR_HEADER_SIZE)))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    uint32_t remaining_length = buffer_length - offset;
    if (xUSB_Descriptor_Read_Header(&buffer[offset], remaining_length, &descriptor_length, &descriptor_type) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    if ((descriptor_length < xUSB_DESCRIPTOR_HEADER_SIZE) || ((uint32_t)descriptor_length > remaining_length))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    header->data = &buffer[offset];
    header->offset = offset;
    header->length = descriptor_length;
    header->type = descriptor_type;

    return xRETURN_OK;
}

static xRETURN_t setup_request_build(USB_Setup_Request_t *request,
                                     uint8_t request_type,
                                     uint8_t request_code,
                                     uint16_t value,
                                     uint16_t index,
                                     uint16_t length)
{
    if (request == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    request->bRequestType = request_type;
    request->bRequest = request_code;
    xUSB_Setup_Set_Value(request, value);
    xUSB_Setup_Set_Index(request, index);
    xUSB_Setup_Set_Length(request, length);

    return xRETURN_OK;
}

static xRETURN_t setup_recipient_validate(uint8_t recipient)
{
    if ((recipient != USB_REQ_RECIPIENT_DEVICE) && (recipient != USB_REQ_RECIPIENT_INTERFACE) && (recipient != USB_REQ_RECIPIENT_ENDPOINT))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_Device_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Device_Descriptor_t *descriptor)
{
    xUSBH_Descriptor_Header_t header = {0};

    if (descriptor == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xRETURN_t status = descriptor_header_parse(buffer, buffer_length, USB_DEVICE_DESC_LEN, USB_DESC_TYPE_DEVICE, &header);
    if (status != xRETURN_OK)
    {
        return status;
    }

    descriptor->bcd_usb = 0U;
    descriptor->vendor_id = 0U;
    descriptor->product_id = 0U;
    descriptor->bcd_device = 0U;

    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, DEVICE_DESC_BCD_USB_OFFSET, &descriptor->bcd_usb);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_CLASS_OFFSET, &descriptor->device_class);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_SUBCLASS_OFFSET, &descriptor->device_subclass);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_PROTOCOL_OFFSET, &descriptor->device_protocol);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_EP0_MPS_OFFSET, &descriptor->ep0_max_packet_size);
    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, DEVICE_DESC_VENDOR_ID_OFFSET, &descriptor->vendor_id);
    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, DEVICE_DESC_PRODUCT_ID_OFFSET, &descriptor->product_id);
    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, DEVICE_DESC_BCD_DEVICE_OFFSET, &descriptor->bcd_device);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_MANUFACTURER_OFFSET, &descriptor->manufacturer_string_index);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_PRODUCT_OFFSET, &descriptor->product_string_index);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_SERIAL_OFFSET, &descriptor->serial_string_index);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, DEVICE_DESC_CONFIG_COUNT_OFFSET, &descriptor->configuration_count);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Configuration_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Configuration_Descriptor_t *descriptor)
{
    xUSBH_Descriptor_Header_t header = {0};

    if (descriptor == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xRETURN_t status = descriptor_header_parse(buffer, buffer_length, USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, &header);
    if (status != xRETURN_OK)
    {
        return status;
    }

    descriptor->total_length = 0U;

    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, CONFIG_DESC_TOTAL_LENGTH_OFFSET, &descriptor->total_length);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, CONFIG_DESC_INTERFACE_COUNT_OFFSET, &descriptor->interface_count);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, CONFIG_DESC_VALUE_OFFSET, &descriptor->configuration_value);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, CONFIG_DESC_STRING_OFFSET, &descriptor->configuration_string_index);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, CONFIG_DESC_ATTRIBUTES_OFFSET, &descriptor->attributes);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, CONFIG_DESC_MAX_POWER_OFFSET, &descriptor->max_power);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Interface_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Interface_Descriptor_t *descriptor)
{
    xUSBH_Descriptor_Header_t header = {0};

    if (descriptor == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xRETURN_t status = descriptor_header_parse(buffer, buffer_length, USB_INTERFACE_DESC_LEN, USB_DESC_TYPE_INTERFACE, &header);
    if (status != xRETURN_OK)
    {
        return status;
    }

    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_NUMBER_OFFSET, &descriptor->interface_number);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_ALT_SETTING_OFFSET, &descriptor->alternate_setting);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_ENDPOINT_COUNT_OFFSET, &descriptor->endpoint_count);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_CLASS_OFFSET, &descriptor->class_code);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_SUBCLASS_OFFSET, &descriptor->subclass);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_PROTOCOL_OFFSET, &descriptor->protocol);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, INTERFACE_DESC_STRING_OFFSET, &descriptor->interface_string_index);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Endpoint_Descriptor_Parse(const uint8_t *buffer, uint32_t buffer_length, xUSBH_Endpoint_Descriptor_t *descriptor)
{
    xUSBH_Descriptor_Header_t header = {0};

    if (descriptor == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xRETURN_t status = descriptor_header_parse(buffer, buffer_length, USB_ENDPOINT_DESC_LEN, USB_DESC_TYPE_ENDPOINT, &header);
    if (status != xRETURN_OK)
    {
        return status;
    }

    descriptor->max_packet_size = 0U;

    (void)xUSB_Descriptor_Read_U8(header.data, header.length, ENDPOINT_DESC_ADDRESS_OFFSET, &descriptor->endpoint_address);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, ENDPOINT_DESC_ATTRIBUTES_OFFSET, &descriptor->attributes);
    descriptor->endpoint_type = descriptor->attributes & USB_ENDP_TYPE_MASK;
    (void)xUSB_Descriptor_Read_LE16(header.data, header.length, ENDPOINT_DESC_MAX_PACKET_SIZE_OFFSET, &descriptor->max_packet_size);
    (void)xUSB_Descriptor_Read_U8(header.data, header.length, ENDPOINT_DESC_INTERVAL_OFFSET, &descriptor->interval);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Configuration_Descriptor_Validate(const uint8_t *buffer, uint32_t buffer_length, uint16_t *total_length)
{
    xUSBH_Configuration_Descriptor_t config = {0};
    xUSBH_Descriptor_Walker_t walker = {0};
    xUSBH_Descriptor_Header_t header = {0};
    bool has_descriptor = false;

    if (total_length == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    xRETURN_t status = xUSBH_Configuration_Descriptor_Parse(buffer, buffer_length, &config);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if ((config.total_length < USB_CONFIGURATION_DESC_LEN) || ((uint32_t)config.total_length > buffer_length))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    status = xUSBH_Descriptor_Walker_Init(&walker, buffer, config.total_length);
    while (status == xRETURN_OK)
    {
        status = xUSBH_Descriptor_Walker_Next(&walker, &header, &has_descriptor);
        if ((status != xRETURN_OK) || (has_descriptor == false))
        {
            break;
        }
    }

    if (status != xRETURN_OK)
    {
        return status;
    }

    *total_length = config.total_length;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Descriptor_Walker_Init(xUSBH_Descriptor_Walker_t *walker, const uint8_t *buffer, uint32_t buffer_length)
{
    if ((walker == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    walker->buffer = buffer;
    walker->length = buffer_length;
    walker->offset = 0U;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Descriptor_Walker_Next(xUSBH_Descriptor_Walker_t *walker, xUSBH_Descriptor_Header_t *descriptor, bool *has_descriptor)
{
    if ((walker == NULL) || (descriptor == NULL) || (has_descriptor == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    *has_descriptor = false;

    if (walker->offset >= walker->length)
    {
        return xRETURN_OK;
    }

    xRETURN_t status = descriptor_header_parse_at(walker->buffer, walker->length, walker->offset, descriptor);
    if (status != xRETURN_OK)
    {
        return status;
    }

    walker->offset += descriptor->length;
    *has_descriptor = true;

    return xRETURN_OK;
}

xRETURN_t xUSBH_Setup_Build_Get_Descriptor(USB_Setup_Request_t *request,
                                           uint8_t descriptor_type,
                                           uint8_t descriptor_index,
                                           uint16_t index,
                                           uint16_t length)
{
    uint16_t value = ((uint16_t)descriptor_type << USB_SETUP_DESCRIPTOR_VALUE_TYPE_SHIFT) | descriptor_index;

    return setup_request_build(request, USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, USB_REQ_GET_DESCRIPTOR, value,
                               index, length);
}

xRETURN_t xUSBH_Setup_Build_Set_Address(USB_Setup_Request_t *request, uint8_t address)
{
    if (address > USB_STANDARD_DEVICE_ADDRESS_MAX)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    return setup_request_build(request, USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, USB_REQ_SET_ADDRESS, address,
                               0U, 0U);
}

xRETURN_t xUSBH_Setup_Build_Get_Configuration(USB_Setup_Request_t *request)
{
    return setup_request_build(request, USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, USB_REQ_GET_CONFIGURATION, 0U,
                               0U, 1U);
}

xRETURN_t xUSBH_Setup_Build_Set_Configuration(USB_Setup_Request_t *request, uint8_t configuration_value)
{
    return setup_request_build(request, USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, USB_REQ_SET_CONFIGURATION,
                               configuration_value, 0U, 0U);
}

xRETURN_t xUSBH_Setup_Build_Get_Status(USB_Setup_Request_t *request, uint8_t recipient, uint16_t index)
{
    xRETURN_t status = setup_recipient_validate(recipient);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return setup_request_build(request, USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | recipient, USB_REQ_GET_STATUS, 0U, index,
                               USB_GET_STATUS_RESPONSE_SIZE);
}

xRETURN_t xUSBH_Setup_Build_Clear_Feature(USB_Setup_Request_t *request, uint8_t recipient, uint16_t feature, uint16_t index)
{
    xRETURN_t status = setup_recipient_validate(recipient);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return setup_request_build(request, USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | recipient, USB_REQ_CLEAR_FEATURE, feature, index, 0U);
}

xRETURN_t xUSBH_Setup_Build_Set_Feature(USB_Setup_Request_t *request, uint8_t recipient, uint16_t feature, uint16_t index)
{
    xRETURN_t status = setup_recipient_validate(recipient);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return setup_request_build(request, USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | recipient, USB_REQ_SET_FEATURE, feature, index, 0U);
}

xRETURN_t xUSBH_Setup_Build_Set_Interface(USB_Setup_Request_t *request, uint8_t interface_number, uint8_t alternate_setting)
{
    return setup_request_build(request, USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE, USB_REQ_SET_INTERFACE,
                               alternate_setting, interface_number, 0U);
}

// EOF /////////////////////////////////////////////////////////////////////////////
