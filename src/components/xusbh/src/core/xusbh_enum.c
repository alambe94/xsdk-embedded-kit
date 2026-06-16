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

// @file xusbh_enum.c
// @brief xUSBH direct-attached-device enumeration state machine.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_descriptor.h"
#include "xusb_setup.h"
#include "xusbh_class.h"
#include "xusbh_descriptor.h"
#include "xusbh_enum.h"
#include "xusbh_trace.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH 8U
#define USBH_ENUM_DEFAULT_CONFIG_INDEX            0U
#define USBH_ENUM_CONTROL_ENDPOINT_ADDRESS        0U
#define USBH_ENUM_DEFAULT_CONFIGURATION_VALUE     1U
#define USBH_ENUM_DEVICE_ADDRESS_BASE             1U
#define USBH_SUPER_SPEED_EP0_PACKET_SIZE_CODE     9U
#define USBH_SUPER_SPEED_EP0_PACKET_SIZE          512U

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static bool root_port_is_valid(const xUSBH_Context_t *host_ctx, uint8_t port);
static bool device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Device_Context_t *device_ctx, uint8_t *index);
static bool transfer_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *index);
static xUSBH_Device_Context_t *enum_device_get(xUSBH_Context_t *host_ctx);
static xUSBH_Transfer_t *enum_transfer_get(xUSBH_Context_t *host_ctx);
static void enum_state_set(xUSBH_Context_t *host_ctx, xUSBH_Enumeration_State_t state);
static xRETURN_t enum_fail(xUSBH_Context_t *host_ctx, xRETURN_t status);
static xRETURN_t enum_transfer_release(xUSBH_Context_t *host_ctx);
static xRETURN_t ep0_packet_size_decode(USB_Speed_t speed, uint8_t descriptor_value, uint16_t *packet_size);
static xRETURN_t enum_control_submit(xUSBH_Context_t *host_ctx,
                                     const USB_Setup_Request_t *setup,
                                     uint8_t *data,
                                     uint32_t length,
                                     xUSBH_Enumeration_State_t wait_state);
static xRETURN_t
enum_wait_for_transfer(xUSBH_Context_t *host_ctx, uint32_t expected_length, xRETURN_t (*complete)(xUSBH_Context_t *host_ctx));
static xRETURN_t enum_device_header_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_set_address_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_device_full_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_config_header_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_config_full_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_set_configuration_complete(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_get_device_header_submit(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_set_address_submit(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_get_device_full_submit(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_get_config_header_submit(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_get_config_full_submit(xUSBH_Context_t *host_ctx);
static xRETURN_t enum_set_configuration_submit(xUSBH_Context_t *host_ctx);

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static bool root_port_is_valid(const xUSBH_Context_t *host_ctx, uint8_t port)
{
    return (host_ctx != NULL) && (port < host_ctx->root_port_count);
}

static bool device_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Device_Context_t *device_ctx, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if (&host_ctx->devices[i] == device_ctx)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static bool transfer_index_get(const xUSBH_Context_t *host_ctx, const xUSBH_Transfer_t *transfer, uint8_t *index)
{
    uint8_t i;

    for (i = 0U; i < xUSBH_MAX_TRANSFERS; i++)
    {
        if (&host_ctx->transfers[i] == transfer)
        {
            *index = i;
            return true;
        }
    }

    return false;
}

static xUSBH_Device_Context_t *enum_device_get(xUSBH_Context_t *host_ctx)
{
    if ((host_ctx == NULL) || (host_ctx->enumeration.device_index >= xUSBH_MAX_DEVICES) ||
        (host_ctx->devices[host_ctx->enumeration.device_index].is_allocated == false))
    {
        return NULL;
    }

    return &host_ctx->devices[host_ctx->enumeration.device_index];
}

static xUSBH_Transfer_t *enum_transfer_get(xUSBH_Context_t *host_ctx)
{
    if ((host_ctx == NULL) || (host_ctx->enumeration.transfer_index >= xUSBH_MAX_TRANSFERS) ||
        (host_ctx->transfers[host_ctx->enumeration.transfer_index].is_allocated == false))
    {
        return NULL;
    }

    return &host_ctx->transfers[host_ctx->enumeration.transfer_index];
}

static void enum_state_set(xUSBH_Context_t *host_ctx, xUSBH_Enumeration_State_t state)
{
    if (host_ctx == NULL)
    {
        return;
    }

    host_ctx->enumeration.state = state;
    xUSBH_TRACE_E2(host_ctx, xUSBH_TRACE_CODE_ENUM_STATE, host_ctx->enumeration.port, state);
}

static xRETURN_t enum_transfer_release(xUSBH_Context_t *host_ctx)
{
    xUSBH_Transfer_t *transfer = enum_transfer_get(host_ctx);
    if (transfer == NULL)
    {
        host_ctx->enumeration.has_transfer = false;
        return xRETURN_OK;
    }

    xRETURN_t status = xUSBH_Transfer_Release(host_ctx, transfer);
    host_ctx->enumeration.has_transfer = false;

    return status;
}

static xRETURN_t enum_fail(xUSBH_Context_t *host_ctx, xRETURN_t status)
{
    if (host_ctx != NULL)
    {
        xUSBH_TRACE_E2(host_ctx, xUSBH_TRACE_CODE_ENUM_ERROR, host_ctx->enumeration.state, status);
        if (root_port_is_valid(host_ctx, host_ctx->enumeration.port) == true)
        {
            host_ctx->root_ports[host_ctx->enumeration.port].state = xUSBH_ROOT_PORT_ERROR;
        }

        (void)enum_transfer_release(host_ctx);
        host_ctx->enumeration.is_active = false;
        enum_state_set(host_ctx, xUSBH_ENUMERATION_ERROR);
    }

    return status;
}

static xRETURN_t ep0_packet_size_decode(USB_Speed_t speed, uint8_t descriptor_value, uint16_t *packet_size)
{
    if (packet_size == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    switch (speed)
    {
    case USB_SPEED_LOW:
        if (descriptor_value == 8U)
        {
            *packet_size = 8U;
            return xRETURN_OK;
        }
        break;

    case USB_SPEED_FULL:
        if ((descriptor_value == 8U) || (descriptor_value == 16U) || (descriptor_value == 32U) || (descriptor_value == 64U))
        {
            *packet_size = descriptor_value;
            return xRETURN_OK;
        }
        break;

    case USB_SPEED_HIGH:
        if (descriptor_value == 64U)
        {
            *packet_size = 64U;
            return xRETURN_OK;
        }
        break;

    case USB_SPEED_SUPER:
        if (descriptor_value == USBH_SUPER_SPEED_EP0_PACKET_SIZE_CODE)
        {
            *packet_size = USBH_SUPER_SPEED_EP0_PACKET_SIZE;
            return xRETURN_OK;
        }
        break;

    default:
        break;
    }

    return xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR;
}

static xRETURN_t enum_control_submit(xUSBH_Context_t *host_ctx,
                                     const USB_Setup_Request_t *setup,
                                     uint8_t *data,
                                     uint32_t length,
                                     xUSBH_Enumeration_State_t wait_state)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    xUSBH_Transfer_t *transfer = enum_transfer_get(host_ctx);

    if ((device == NULL) || (transfer == NULL) || (setup == NULL))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    transfer->device_address = device->address;
    transfer->endpoint_address = USBH_ENUM_CONTROL_ENDPOINT_ADDRESS;
    transfer->endpoint_type = USB_ENDP_TYPE_CTRL;
    transfer->interval = 0U;
    transfer->has_setup = true;
    transfer->setup = *setup;
    transfer->last_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE;
    transfer->data = data;
    transfer->length = length;
    transfer->actual_length = 0U;
    transfer->user_ctx = &host_ctx->enumeration;

    xRETURN_t status = xUSBH_Transfer_Submit(host_ctx, transfer);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    host_ctx->enumeration.timeout_remaining = xUSBH_CONTROL_TRANSFER_TIMEOUT_TICKS;
    enum_state_set(host_ctx, wait_state);

    return xRETURN_OK;
}

static xRETURN_t
enum_wait_for_transfer(xUSBH_Context_t *host_ctx, uint32_t expected_length, xRETURN_t (*complete)(xUSBH_Context_t *host_ctx))
{
    xUSBH_Transfer_t *transfer = enum_transfer_get(host_ctx);
    if ((transfer == NULL) || (complete == NULL))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    if (transfer->is_submitted == true)
    {
        if (host_ctx->enumeration.timeout_remaining == 0U)
        {
            return enum_fail(host_ctx, xRETURN_xERR_xUSBH_TIMEOUT);
        }

        host_ctx->enumeration.timeout_remaining--;
        return xRETURN_OK;
    }

    if ((transfer->last_event != xUSBH_HCD_TRANSFER_EVENT_COMPLETE) || (transfer->actual_length < expected_length))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    return complete(host_ctx);
}

static xRETURN_t enum_device_header_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    uint8_t descriptor_length = 0U;
    uint8_t descriptor_type = 0U;
    uint8_t ep0_packet_size_value = 0U;
    uint16_t ep0_packet_size = 0U;

    if (device == NULL)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    if ((xUSB_Descriptor_Read_Header(host_ctx->control_buffer, USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH, &descriptor_length,
                                     &descriptor_type) == false) ||
        (descriptor_length < USB_DEVICE_DESC_LEN) || (descriptor_type != USB_DESC_TYPE_DEVICE) ||
        (xUSB_Descriptor_Read_U8(host_ctx->control_buffer, USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH, 7U, &ep0_packet_size_value) == false))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    xRETURN_t status = ep0_packet_size_decode(device->speed, ep0_packet_size_value, &ep0_packet_size);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    device->ep0_max_packet_size = ep0_packet_size;
    enum_state_set(host_ctx, xUSBH_ENUMERATION_SET_ADDRESS_SUBMIT);

    return xRETURN_OK;
}

static xRETURN_t enum_set_address_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    if (device == NULL)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    device->address = host_ctx->enumeration.assigned_address;
    device->state = xUSBH_DEVICE_STATE_ADDRESSED;
    host_ctx->enumeration.address_settle_remaining = xUSBH_ADDRESS_SETTLE_TICKS;
    enum_state_set(host_ctx, xUSBH_ENUMERATION_ADDRESS_SETTLE);

    return xRETURN_OK;
}

static xRETURN_t enum_device_full_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    xUSBH_Device_Descriptor_t descriptor = {0};

    if (device == NULL)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    xRETURN_t status = xUSBH_Device_Descriptor_Parse(host_ctx->control_buffer, USB_DEVICE_DESC_LEN, &descriptor);
    if ((status != xRETURN_OK) || (descriptor.configuration_count == 0U))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    device->vendor_id = descriptor.vendor_id;
    device->product_id = descriptor.product_id;
    device->device_class = descriptor.device_class;
    device->device_subclass = descriptor.device_subclass;
    device->device_protocol = descriptor.device_protocol;
    enum_state_set(host_ctx, xUSBH_ENUMERATION_GET_CONFIG_HEADER_SUBMIT);

    return xRETURN_OK;
}

static xRETURN_t enum_config_header_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Configuration_Descriptor_t descriptor = {0};
    xRETURN_t status = xUSBH_Configuration_Descriptor_Parse(host_ctx->control_buffer, USB_CONFIGURATION_DESC_LEN, &descriptor);
    if ((status != xRETURN_OK) || (descriptor.total_length < USB_CONFIGURATION_DESC_LEN) ||
        ((uint32_t)descriptor.total_length > xUSBH_MAX_CONFIG_DESCRIPTOR_SIZE) || (descriptor.interface_count == 0U))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    host_ctx->enumeration.config_total_length = descriptor.total_length;
    host_ctx->enumeration.configuration_value = descriptor.configuration_value;
    if (host_ctx->enumeration.configuration_value == 0U)
    {
        host_ctx->enumeration.configuration_value = USBH_ENUM_DEFAULT_CONFIGURATION_VALUE;
    }
    enum_state_set(host_ctx, xUSBH_ENUMERATION_GET_CONFIG_FULL_SUBMIT);

    return xRETURN_OK;
}

static xRETURN_t enum_config_full_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    uint16_t total_length = 0U;

    if (device == NULL)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    xRETURN_t status =
        xUSBH_Configuration_Descriptor_Validate(host_ctx->control_buffer, host_ctx->enumeration.config_total_length, &total_length);
    if ((status != xRETURN_OK) || (total_length != host_ctx->enumeration.config_total_length))
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    status = xUSBH_Device_Build_Topology(host_ctx, device, host_ctx->control_buffer, host_ctx->enumeration.config_total_length);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR);
    }

    enum_state_set(host_ctx, xUSBH_ENUMERATION_SET_CONFIGURATION_SUBMIT);

    return xRETURN_OK;
}

static xRETURN_t enum_set_configuration_complete(xUSBH_Context_t *host_ctx)
{
    xUSBH_Device_Context_t *device = enum_device_get(host_ctx);
    if (device == NULL)
    {
        return enum_fail(host_ctx, xRETURN_xERR_xUSBH_INVALID_OBJECT);
    }

    xRETURN_t status = enum_transfer_release(host_ctx);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    status = xUSBH_Class_Bind_Device(host_ctx, device);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    device->active_configuration_value = host_ctx->enumeration.configuration_value;
    device->is_configured = true;
    device->state = xUSBH_DEVICE_STATE_CONFIGURED;

    host_ctx->root_ports[host_ctx->enumeration.port].state = xUSBH_ROOT_PORT_CONFIGURED;
    host_ctx->enumeration.is_active = false;
    enum_state_set(host_ctx, xUSBH_ENUMERATION_COMPLETE);

    return xRETURN_OK;
}

static xRETURN_t enum_get_device_header_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Get_Descriptor(&setup, USB_DESC_TYPE_DEVICE, 0U, 0U, USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, host_ctx->control_buffer, USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH,
                               xUSBH_ENUMERATION_GET_DEVICE_HEADER_WAIT);
}

static xRETURN_t enum_set_address_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Set_Address(&setup, host_ctx->enumeration.assigned_address);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, NULL, 0U, xUSBH_ENUMERATION_SET_ADDRESS_WAIT);
}

static xRETURN_t enum_get_device_full_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Get_Descriptor(&setup, USB_DESC_TYPE_DEVICE, 0U, 0U, USB_DEVICE_DESC_LEN);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, host_ctx->control_buffer, USB_DEVICE_DESC_LEN, xUSBH_ENUMERATION_GET_DEVICE_FULL_WAIT);
}

static xRETURN_t enum_get_config_header_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Get_Descriptor(&setup, USB_DESC_TYPE_CONFIGURATION, USBH_ENUM_DEFAULT_CONFIG_INDEX, 0U,
                                                        USB_CONFIGURATION_DESC_LEN);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, host_ctx->control_buffer, USB_CONFIGURATION_DESC_LEN,
                               xUSBH_ENUMERATION_GET_CONFIG_HEADER_WAIT);
}

static xRETURN_t enum_get_config_full_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Get_Descriptor(&setup, USB_DESC_TYPE_CONFIGURATION, USBH_ENUM_DEFAULT_CONFIG_INDEX, 0U,
                                                        host_ctx->enumeration.config_total_length);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, host_ctx->control_buffer, host_ctx->enumeration.config_total_length,
                               xUSBH_ENUMERATION_GET_CONFIG_FULL_WAIT);
}

static xRETURN_t enum_set_configuration_submit(xUSBH_Context_t *host_ctx)
{
    USB_Setup_Request_t setup = {0};
    xRETURN_t status = xUSBH_Setup_Build_Set_Configuration(&setup, host_ctx->enumeration.configuration_value);
    if (status != xRETURN_OK)
    {
        return enum_fail(host_ctx, status);
    }

    return enum_control_submit(host_ctx, &setup, NULL, 0U, xUSBH_ENUMERATION_SET_CONFIGURATION_WAIT);
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
xRETURN_t xUSBH_Enumeration_Start(xUSBH_Context_t *host_ctx, uint8_t port)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Transfer_t *transfer = NULL;
    uint8_t device_index = 0U;
    uint8_t transfer_index = 0U;

    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (root_port_is_valid(host_ctx, port) == false)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    if (host_ctx->enumeration.is_active == true)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    xRETURN_t status = xUSBH_Device_Allocate(host_ctx, port, &device);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBH_Transfer_Allocate(host_ctx, &transfer);
    if (status != xRETURN_OK)
    {
        (void)xUSBH_Device_Release(host_ctx, device);
        return status;
    }

    if ((device_index_get(host_ctx, device, &device_index) == false) || (transfer_index_get(host_ctx, transfer, &transfer_index) == false))
    {
        (void)xUSBH_Transfer_Release(host_ctx, transfer);
        (void)xUSBH_Device_Release(host_ctx, device);
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    (void)memset(&host_ctx->enumeration, 0, sizeof(host_ctx->enumeration));
    device->address = 0U;
    device->speed = host_ctx->root_ports[port].speed;
    host_ctx->root_ports[port].has_device = true;
    host_ctx->root_ports[port].device_index = device_index;
    host_ctx->enumeration.is_active = true;
    host_ctx->enumeration.has_transfer = true;
    host_ctx->enumeration.port = port;
    host_ctx->enumeration.device_index = device_index;
    host_ctx->enumeration.transfer_index = transfer_index;
    host_ctx->enumeration.assigned_address = (uint8_t)(device_index + USBH_ENUM_DEVICE_ADDRESS_BASE);
    enum_state_set(host_ctx, xUSBH_ENUMERATION_GET_DEVICE_HEADER_SUBMIT);

    return xRETURN_OK;
}

xRETURN_t xUSBH_Enumeration_Process(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if (host_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((root_port_is_valid(host_ctx, port) == false) || (host_ctx->enumeration.port != port))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    if (host_ctx->enumeration.is_active == false)
    {
        return xRETURN_OK;
    }

    switch (host_ctx->enumeration.state)
    {
    case xUSBH_ENUMERATION_GET_DEVICE_HEADER_SUBMIT:
        return enum_get_device_header_submit(host_ctx);

    case xUSBH_ENUMERATION_GET_DEVICE_HEADER_WAIT:
        return enum_wait_for_transfer(host_ctx, USBH_ENUM_DEVICE_DESCRIPTOR_HEADER_LENGTH, enum_device_header_complete);

    case xUSBH_ENUMERATION_SET_ADDRESS_SUBMIT:
        return enum_set_address_submit(host_ctx);

    case xUSBH_ENUMERATION_SET_ADDRESS_WAIT:
        return enum_wait_for_transfer(host_ctx, 0U, enum_set_address_complete);

    case xUSBH_ENUMERATION_ADDRESS_SETTLE:
        if (host_ctx->enumeration.address_settle_remaining > 0U)
        {
            host_ctx->enumeration.address_settle_remaining--;
            return xRETURN_OK;
        }
        enum_state_set(host_ctx, xUSBH_ENUMERATION_GET_DEVICE_FULL_SUBMIT);
        return enum_get_device_full_submit(host_ctx);

    case xUSBH_ENUMERATION_GET_DEVICE_FULL_SUBMIT:
        return enum_get_device_full_submit(host_ctx);

    case xUSBH_ENUMERATION_GET_DEVICE_FULL_WAIT:
        return enum_wait_for_transfer(host_ctx, USB_DEVICE_DESC_LEN, enum_device_full_complete);

    case xUSBH_ENUMERATION_GET_CONFIG_HEADER_SUBMIT:
        return enum_get_config_header_submit(host_ctx);

    case xUSBH_ENUMERATION_GET_CONFIG_HEADER_WAIT:
        return enum_wait_for_transfer(host_ctx, USB_CONFIGURATION_DESC_LEN, enum_config_header_complete);

    case xUSBH_ENUMERATION_GET_CONFIG_FULL_SUBMIT:
        return enum_get_config_full_submit(host_ctx);

    case xUSBH_ENUMERATION_GET_CONFIG_FULL_WAIT:
        return enum_wait_for_transfer(host_ctx, host_ctx->enumeration.config_total_length, enum_config_full_complete);

    case xUSBH_ENUMERATION_SET_CONFIGURATION_SUBMIT:
        return enum_set_configuration_submit(host_ctx);

    case xUSBH_ENUMERATION_SET_CONFIGURATION_WAIT:
        return enum_wait_for_transfer(host_ctx, 0U, enum_set_configuration_complete);

    case xUSBH_ENUMERATION_IDLE:
    case xUSBH_ENUMERATION_COMPLETE:
    case xUSBH_ENUMERATION_ERROR:
    default:
        return xRETURN_OK;
    }
}

void xUSBH_Enumeration_Abort(xUSBH_Context_t *host_ctx, uint8_t port)
{
    if ((host_ctx == NULL) || (root_port_is_valid(host_ctx, port) == false) || (host_ctx->enumeration.port != port))
    {
        return;
    }

    (void)enum_transfer_release(host_ctx);
    (void)memset(&host_ctx->enumeration, 0, sizeof(host_ctx->enumeration));
    enum_state_set(host_ctx, xUSBH_ENUMERATION_IDLE);
}

// EOF /////////////////////////////////////////////////////////////////////////////
