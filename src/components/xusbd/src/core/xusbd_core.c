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

// @file xusbd_core.c
// @brief USB device core lifecycle functions and DCD event dispatcher.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbd_core.h"
#include "xusbd_std.h"
#include "xusbd_config.h"
#include "xusbd_return.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t dcd_speed_transition(xUSBD_Device_Context_t *device_ctx, USB_DCD_Event_t event);
static xRETURN_t dcd_connect_event_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t dcd_speed_change_event_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t dcd_link_state_event_process(xUSBD_Device_Context_t *device_ctx, uint8_t *data, uint32_t length);
static xRETURN_t dcd_reset_event_process(xUSBD_Device_Context_t *device_ctx);
static bool is_required_string_missing(const uint8_t *string);
static bool dcd_ops_complete(const xUSBD_DCD_Ops_t *ops);
static bool descriptors_initialized(const xUSBD_Device_Context_t *device_ctx);
static xRETURN_t validate_start_configuration(xUSBD_Device_Context_t *device_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static inline uint16_t usb_bcd_version(USB_Speed_t speed)
{
    switch (speed)
    {
    case USB_SPEED_SUPER:
        return 0x0300U;
    case USB_SPEED_HIGH:
        return 0x0210U;
    case USB_SPEED_FULL:
        return 0x0110U;
    default:
        return 0x0100U;
    }
}

static bool is_required_string_missing(const uint8_t *string)
{
    return (string == NULL || string[0] == '\0');
}

static bool dcd_ops_complete(const xUSBD_DCD_Ops_t *ops)
{
    return (ops->init != NULL) && (ops->set_event_callback != NULL) && (ops->connect != NULL) && (ops->enable_interrupts != NULL) &&
           (ops->get_speed != NULL) && (ops->ep_init != NULL) && (ops->ep_receive != NULL) && (ops->ep_send != NULL) &&
           (ops->ep_stall != NULL);
}

static bool descriptors_initialized(const xUSBD_Device_Context_t *device_ctx)
{
    const USB_Device_Descriptor_t *desc = (const USB_Device_Descriptor_t *)device_ctx->device_descriptor;
    const USB_Device_Qualifier_Descriptor_t *qdesc = (const USB_Device_Qualifier_Descriptor_t *)device_ctx->device_qualifier_descriptor;

    return (desc->bLength == 18U) && (desc->bDescriptorType == USB_DESC_TYPE_DEVICE) && (desc->bNumConfigurations != 0U) &&
           (qdesc->bLength == 10U) && (qdesc->bDescriptorType == USB_DESC_TYPE_QUALIFIER);
}

static xRETURN_t validate_start_configuration(xUSBD_Device_Context_t *device_ctx)
{
    if (device_ctx == NULL || device_ctx->dcd_ops == NULL || device_ctx->dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (dcd_ops_complete(device_ctx->dcd_ops) == false)
    {
        return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
    }

    if (is_required_string_missing(device_ctx->vendor_string) || is_required_string_missing(device_ctx->product_string) ||
        is_required_string_missing(device_ctx->serial_string))
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    if (descriptors_initialized(device_ctx) == false)
    {
        return xRETURN_xERR_xUSBD_NOT_INITIALIZED;
    }

    if (device_ctx->class_list_head == NULL || device_ctx->class_list_tail == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    return xRETURN_OK;
}

static xRETURN_t dcd_speed_transition(xUSBD_Device_Context_t *device_ctx, USB_DCD_Event_t event)
{
    USB_Speed_t speed = USB_SPEED_FULL;
    xRETURN_t status = xUSBD_DCD_Get_Speed(device_ctx->dcd_ops, device_ctx->dcd_ctx, &speed);
    if (status != xRETURN_OK)
    {
        return status;
    }
    device_ctx->speed = speed;
    status = xUSBD_EP0_Configure(device_ctx, speed);
    if (status == xRETURN_OK)
    {
        status = xUSBD_Class_Bus_Event_Process(device_ctx, event);
    }
    return status;
}

static xRETURN_t dcd_connect_event_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = dcd_speed_transition(device_ctx, USB_DCD_CONNECT_RECEIVED);
    if (status == xRETURN_OK)
    {
        xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_BUS_CONNECT, device_ctx->speed);
    }
    return status;
}

static xRETURN_t dcd_speed_change_event_process(xUSBD_Device_Context_t *device_ctx)
{
    return dcd_speed_transition(device_ctx, USB_DCD_SPEED_CHANGE_RECEIVED);
}

static xRETURN_t dcd_link_state_event_process(xUSBD_Device_Context_t *device_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_DCD_Link_State_Event_t link_event = {
        .link_state = USB_DCD_LINK_STATE_UNKNOWN,
    };

    if (data != NULL && length >= sizeof(link_event))
    {
        memcpy(&link_event, data, sizeof(link_event));
    }

    device_ctx->link_state = link_event.link_state;
    return xUSBD_Class_Bus_Event_Process(device_ctx, USB_DCD_LINK_STATE_CHANGE_RECEIVED);
}

static xRETURN_t dcd_reset_event_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status;

    device_ctx->is_addressed = false;
    device_ctx->address_value = 0U;
    device_ctx->is_configured = false;
    device_ctx->configuration_value = 0U;
    device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
    device_ctx->lifecycle_state = xUSBD_LIFECYCLE_STARTED;

    status = xUSBD_Class_Bus_Event_Process(device_ctx, USB_DCD_RESET_RECEIVED);
    if (status == xRETURN_OK)
    {
        xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_BUS_RESET, device_ctx->speed);
    }

    return status;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_DCD_Event_Callback(void *device_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_Device_Context_t *ctx = (xUSBD_Device_Context_t *)device_ctx;
    xRETURN_t status = xRETURN_OK;

    if (ctx == NULL)
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_NULL_POINTER, "NULL pointer exception");
        return;
    }

    switch (event)
    {
    case USB_DCD_SETUP_RECEIVED:
        if (length == 8 && data != NULL)
        {
            USB_Setup_Request_t *request = (USB_Setup_Request_t *)data;

            // Copy and normalize to host byte order at the DCD boundary.
            // All downstream code reads ctx->request fields as plain host-order values.
            ctx->request.bRequestType = request->bRequestType;
            ctx->request.bRequest = request->bRequest;
            ctx->request.wValue = xLE16_TO_CPU(request->wValue);
            ctx->request.wIndex = xLE16_TO_CPU(request->wIndex);
            ctx->request.wLength = xLE16_TO_CPU(request->wLength);

            status = xUSBD_EP0_Setup_Process(ctx);
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_INVALID_SETUP_PACKET, "Invalid setup packet");
            status = xRETURN_xERR_xUSBD_INVALID_SETUP_PACKET;
        }
        break;
    case USB_DCD_DATA_RECEIVED:
        if (ep_addr == 0x00)
        {
            status = xUSBD_EP0_Data_Received_Process(ctx, length);
        }
        else
        {
            status = xUSBD_Class_Data_Received(ctx, ep_addr, data, length);
        }
        break;
    case USB_DCD_DATA_SENT:
        if (ep_addr == 0x80)
        {
            status = xUSBD_EP0_Data_Sent_Process(ctx);
        }
        else
        {
            status = xUSBD_Class_Data_Sent(ctx, ep_addr, data, length);
        }
        break;
    case USB_DCD_CONNECT_RECEIVED:
        status = dcd_connect_event_process(ctx);
        break;
    case USB_DCD_SPEED_CHANGE_RECEIVED:
        status = dcd_speed_change_event_process(ctx);
        break;
    case USB_DCD_LINK_STATE_CHANGE_RECEIVED:
        status = dcd_link_state_event_process(ctx, data, length);
        break;
    case USB_DCD_RESET_RECEIVED:
        status = dcd_reset_event_process(ctx);
        break;
    case USB_DCD_SOF_RECEIVED:
    case USB_DCD_SUSPEND_RECEIVED:
    case USB_DCD_RESUME_RECEIVED:
    case USB_DCD_DISCONNECT_RECEIVED:
        if (event == USB_DCD_DISCONNECT_RECEIVED)
        {
            ctx->link_state = USB_DCD_LINK_STATE_DISABLED;
        }
        status = xUSBD_Class_Bus_Event_Process(ctx, event);
        if ((status == xRETURN_OK) && (event == USB_DCD_DISCONNECT_RECEIVED))
        {
            xUSBD_TRACE_E1(ctx, xUSBD_TRACE_CODE_BUS_DISCONNECT, 0U);
        }
        break;
    default:
        break;
    }

    if (status != xRETURN_OK)
    {
        xUSBD_TRACE_E1(ctx, xUSBD_TRACE_CODE_DCD_ERROR, status);
        xUSBD_LOG(status, "USB device event failed");
    }
}

#if xTRACE_ENABLE
xRETURN_t xUSBD_Trace_Init(xUSBD_Device_Context_t *device_ctx, struct xTRACE_Context_t *trace_ctx)
{
    if (device_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    device_ctx->trace_ctx = trace_ctx;
    return xRETURN_OK;
}
#endif

xRETURN_t xUSBD_Init(xUSBD_Device_Context_t *device_ctx, const xUSBD_Init_Config_t *config)
{
    if (device_ctx == NULL || config == NULL || config->vendor_string == NULL || config->product_string == NULL ||
        config->serial_number_string == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    memset(device_ctx, 0, sizeof(*device_ctx));

    device_ctx->speed = config->speed;
    device_ctx->lifecycle_state = xUSBD_LIFECYCLE_INITIALIZED;
    device_ctx->next_in_ep = 0x81U;
    device_ctx->bos_built_for_speed = 0xFFU;
    device_ctx->next_out_ep = 0x01;
    device_ctx->next_string_index = xUSBD_SERIAL_STRING_INDEX + 1;

    strncpy((char *)device_ctx->vendor_string, (const char *)config->vendor_string, xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1);
    device_ctx->vendor_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1] = '\0';
    strncpy((char *)device_ctx->product_string, (const char *)config->product_string, xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1);
    device_ctx->product_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1] = '\0';
    strncpy((char *)device_ctx->serial_string, (const char *)config->serial_number_string, xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1);
    device_ctx->serial_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE - 1] = '\0';

    uint16_t bcd_usb = usb_bcd_version(config->speed);

    USB_Device_Descriptor_t *descriptor = (USB_Device_Descriptor_t *)device_ctx->device_descriptor;
    descriptor->bLength = 18;
    descriptor->bDescriptorType = USB_DESC_TYPE_DEVICE;
    descriptor->bcdUSB = xCPU_TO_LE16(bcd_usb);
    descriptor->bDeviceClass = USB_CLASS_IAD_DEVICE;
    descriptor->bDeviceSubClass = USB_IAD_DEVICE_SUBCLASS;
    descriptor->bDeviceProtocol = USB_IAD_DEVICE_PROTOCOL;
    descriptor->bMaxPacketSize0 = (config->speed == USB_SPEED_SUPER) ? 9 : 64;
    descriptor->idVendor = xCPU_TO_LE16(config->vendor_id);
    descriptor->idProduct = xCPU_TO_LE16(config->product_id);
    descriptor->bcdDevice = xCPU_TO_LE16(bcd_usb);
    descriptor->iManufacturer = xUSBD_VENDOR_STRING_INDEX;
    descriptor->iProduct = xUSBD_PRODUCT_STRING_INDEX;
    descriptor->iSerialNumber = xUSBD_SERIAL_STRING_INDEX;
    descriptor->bNumConfigurations = 0x01;

    USB_Device_Qualifier_Descriptor_t *q_descriptor = (USB_Device_Qualifier_Descriptor_t *)device_ctx->device_qualifier_descriptor;
    q_descriptor->bLength = 10;
    q_descriptor->bDescriptorType = USB_DESC_TYPE_QUALIFIER;
    q_descriptor->bcdUSB = xCPU_TO_LE16(0x0200);
    q_descriptor->bDeviceClass = USB_CLASS_IAD_DEVICE;
    q_descriptor->bDeviceSubClass = USB_IAD_DEVICE_SUBCLASS;
    q_descriptor->bDeviceProtocol = USB_IAD_DEVICE_PROTOCOL;
    q_descriptor->bMaxPacketSize0 = 64;
    q_descriptor->bNumConfigurations = 0x01;
    q_descriptor->bReserved = 0x00;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Start(xUSBD_Device_Context_t *device_ctx, const xUSBD_Start_Config_t *config)
{
    if (device_ctx == NULL || config == NULL || config->dcd_ops == NULL || config->dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    device_ctx->dcd_ops = config->dcd_ops;
    device_ctx->dcd_ctx = config->dcd_ctx;

    xRETURN_t status = validate_start_configuration(device_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    device_ctx->port = config->port;

    // Initialize HW Layer
    status = xUSBD_DCD_Init(device_ctx->dcd_ops, device_ctx->dcd_ctx, device_ctx->speed, device_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBD_DCD_Set_Event_Callback(device_ctx->dcd_ops, device_ctx->dcd_ctx, xUSBD_DCD_Event_Callback);
    if (status != xRETURN_OK)
    {
        return status;
    }

    // Enable usb interrupts
    status = xUSBD_DCD_Enable_Interrupts(device_ctx->dcd_ops, device_ctx->dcd_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    // Attach USB Device
    status = xUSBD_DCD_Connect(device_ctx->dcd_ops, device_ctx->dcd_ctx);
    if (status != xRETURN_OK)
    {
        return status;
    }

    device_ctx->is_started = true;
    device_ctx->lifecycle_state = xUSBD_LIFECYCLE_STARTED;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Stop(xUSBD_Device_Context_t *device_ctx)
{
    if (device_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (!device_ctx->is_started)
    {
        return xRETURN_xERR_xUSBD_NOT_INITIALIZED;
    }

    if (device_ctx->dcd_ops == NULL || device_ctx->dcd_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    // Best-effort all teardown steps - state is reset unconditionally so the
    // context is always consistent after Stop(), even on partial hardware failure.
    xRETURN_t status = xUSBD_DCD_Disable_Interrupts(device_ctx->dcd_ops, device_ctx->dcd_ctx);
    xRETURN_t s2 = xUSBD_DCD_Disconnect(device_ctx->dcd_ops, device_ctx->dcd_ctx);
    xRETURN_t s3 = xUSBD_DCD_Deinit(device_ctx->dcd_ops, device_ctx->dcd_ctx);

    device_ctx->is_started = false;
    device_ctx->is_configured = false;
    device_ctx->is_addressed = false;
    device_ctx->configuration_value = 0U;
    device_ctx->address_value = 0U;
    device_ctx->link_state = USB_DCD_LINK_STATE_DISABLED;
    device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
    device_ctx->lifecycle_state = xUSBD_LIFECYCLE_STOPPED;
    device_ctx->dcd_ops = NULL;
    device_ctx->dcd_ctx = NULL;

    return (status != xRETURN_OK) ? status : (s2 != xRETURN_OK) ? s2 : s3;
}

xRETURN_t xUSBD_Get_Lifecycle_State(const xUSBD_Device_Context_t *device_ctx, xUSBD_Lifecycle_State_t *state)
{
    if (device_ctx == NULL || state == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *state = device_ctx->lifecycle_state;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Is_Started(const xUSBD_Device_Context_t *device_ctx, bool *is_started)
{
    if (device_ctx == NULL || is_started == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *is_started = device_ctx->is_started;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Is_Configured(const xUSBD_Device_Context_t *device_ctx, bool *is_configured)
{
    if (device_ctx == NULL || is_configured == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *is_configured = device_ctx->is_configured;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Get_Address(const xUSBD_Device_Context_t *device_ctx, uint8_t *address)
{
    if (device_ctx == NULL || address == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *address = device_ctx->address_value;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Get_Configuration_Value(const xUSBD_Device_Context_t *device_ctx, uint8_t *configuration_value)
{
    if (device_ctx == NULL || configuration_value == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *configuration_value = device_ctx->configuration_value;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Get_Link_State(const xUSBD_Device_Context_t *device_ctx, USB_DCD_Link_State_t *link_state)
{
    if (device_ctx == NULL || link_state == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *link_state = device_ctx->link_state;
    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
