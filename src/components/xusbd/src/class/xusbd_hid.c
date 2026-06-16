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

// @file xusbd_hid.c
// @brief xUSB Human Interface Device (HID) class driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "string.h"
#include "xusbd_return.h"
#include "xusbd_class.h"
#include "xusbd_hid.h"
#include "xassert.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
xSTATIC_ASSERT(sizeof(USB_HID_Descriptor_t) == USB_HID_DESC_LEN, "USB HID descriptor size changed");

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t hid_init_instance(xUSBD_Class_Context_t *class_ctx);
static uint32_t hid_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
static xRETURN_t hid_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t hid_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t hid_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
static xRETURN_t hid_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t hid_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

static bool hid_config_is_valid(const xUSBD_HID_Context_t *ctx);
static bool hid_request_targets_interface(const xUSBD_HID_Context_t *ctx);
static xRETURN_t hid_call_bus_event_callback(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t hid_init_in_endpoint(xUSBD_Class_Context_t *class_ctx, const xUSBD_HID_Context_t *ctx);
static xRETURN_t hid_deinit_in_endpoint(xUSBD_Class_Context_t *class_ctx, const xUSBD_HID_Context_t *ctx);
static xRETURN_t hid_standard_control_in_request(xUSBD_HID_Context_t *ctx, xUSBD_Response_t *response);
static xRETURN_t hid_class_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Context_t *ctx, xUSBD_Response_t *response);
static xRETURN_t hid_standard_control_out_request(xUSBD_HID_Context_t *ctx);
static xRETURN_t
hid_class_control_out_request(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Context_t *ctx, uint8_t *control_data, uint32_t length);
static inline uint8_t *hid_build_hid_descriptor(uint8_t *p, uint16_t report_length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static bool hid_config_is_valid(const xUSBD_HID_Context_t *ctx)
{
    return (ctx->report_descriptor != NULL) && (ctx->report_descriptor_len > 0U) && (ctx->interval > 0U);
}

static bool hid_request_targets_interface(const xUSBD_HID_Context_t *ctx)
{
    uint16_t w_index = ctx->class_ctx.device_ctx->request.wIndex;
    return xU16_LOW_BYTE(w_index) == ctx->interface;
}

static void hid_set_response(xUSBD_Response_t *response, const uint8_t *data, uint32_t length)
{
    response->data = (uint8_t *)data;
    response->length = length;
}

static inline uint8_t *hid_build_hid_descriptor(uint8_t *p, uint16_t report_length)
{
    USB_HID_Descriptor_t *d = (USB_HID_Descriptor_t *)p;
    d->bLength = USB_HID_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_HID;
    d->bcdHID = xCPU_TO_LE16(0x0111);
    d->bCountryCode = 0x00;
    d->bNumDescriptors = 0x01;
    d->bDescriptorType1 = USB_DESC_TYPE_REPORT;
    d->wItemLength = xCPU_TO_LE16(report_length);
    return p + USB_HID_DESC_LEN;
}

static xRETURN_t hid_call_bus_event_callback(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_HID_Callbacks_t *callbacks = xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_HID_Callbacks_t);
    if ((callbacks != NULL) && (callbacks->on_bus_event != NULL))
    {
        return callbacks->on_bus_event(class_ctx, event);
    }
    return xRETURN_OK;
}

static xRETURN_t hid_init_in_endpoint(xUSBD_Class_Context_t *class_ctx, const xUSBD_HID_Context_t *ctx)
{
    uint16_t ep_mps = 0U;
    xRETURN_t status = xUSBD_Class_Get_EP_MPS(class_ctx, &ep_mps);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return xUSBD_Class_DCD_EP_Init(class_ctx, ctx->in_ep, USB_ENDP_TYPE_INTR, ep_mps);
}

static xRETURN_t hid_deinit_in_endpoint(xUSBD_Class_Context_t *class_ctx, const xUSBD_HID_Context_t *ctx)
{
    return xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->in_ep);
}

static xRETURN_t hid_standard_control_in_request(xUSBD_HID_Context_t *ctx, xUSBD_Response_t *response)
{
    const USB_Setup_Request_t *req = &ctx->class_ctx.device_ctx->request;
    uint8_t descriptor_type = xU16_HIGH_BYTE(req->wValue);

    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
        if (descriptor_type == USB_DESC_TYPE_REPORT)
        {
            hid_set_response(response, ctx->report_descriptor, ctx->report_descriptor_len);
        }
        else if (descriptor_type == USB_DESC_TYPE_HID)
        {
            hid_set_response(response, ctx->hid_descriptor, USB_HID_DESC_LEN);
        }
        break;
    case USB_REQ_GET_INTERFACE:
        hid_set_response(response, &ctx->alt_interface, 1U);
        break;
    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t hid_class_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Context_t *ctx, xUSBD_Response_t *response)
{
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    switch (req->bRequest)
    {
    case USB_HID_REQ_GET_PROTOCOL:
        hid_set_response(response, &ctx->current_protocol, 1U);
        break;
    case USB_HID_REQ_GET_IDLE:
        hid_set_response(response, &ctx->idle_value, 1U);
        break;
    case USB_HID_REQ_GET_REPORT:
    {
        xUSBD_HID_Callbacks_t *callbacks = xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_HID_Callbacks_t);
        if ((callbacks != NULL) && (callbacks->on_get_report != NULL))
        {
            return callbacks->on_get_report(class_ctx, response);
        }
    }
    break;
    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t hid_standard_control_out_request(xUSBD_HID_Context_t *ctx)
{
    const USB_Setup_Request_t *req = &ctx->class_ctx.device_ctx->request;

    if (req->bRequest == USB_REQ_SET_INTERFACE)
    {
        ctx->alt_interface = xU16_LOW_BYTE(req->wValue);
    }

    return xRETURN_OK;
}

static xRETURN_t
hid_class_control_out_request(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Context_t *ctx, uint8_t *control_data, uint32_t length)
{
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    switch (req->bRequest)
    {
    case USB_HID_REQ_SET_PROTOCOL:
        ctx->current_protocol = xU16_LOW_BYTE(req->wValue);
        break;
    case USB_HID_REQ_SET_IDLE:
        ctx->idle_value = xU16_HIGH_BYTE(req->wValue);
        break;
    case USB_HID_REQ_SET_REPORT:
    {
        xUSBD_HID_Callbacks_t *callbacks = xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_HID_Callbacks_t);
        if ((callbacks != NULL) && (callbacks->on_set_report != NULL))
        {
            return callbacks->on_set_report(class_ctx, control_data, length);
        }
    }
    break;
    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t hid_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    if (hid_config_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    xRETURN_t status = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->interface);
    if (status != xRETURN_OK)
    {
        return status;
    }
    status = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_IN, &ctx->in_ep);
    if (status != xRETURN_OK)
    {
        return status;
    }

    ctx->current_protocol = ctx->protocol;
    ctx->idle_value = 0U;
    ctx->alt_interface = 0U;

    return xRETURN_OK;
}

static uint32_t hid_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    uint16_t ep_mps = ep_max_mps(speed, USB_ENDP_TYPE_INTR);
    (void)xUSBD_Class_Set_EP_MPS(class_ctx, ep_mps);
    uint8_t *ptr = buffer;

    ptr = build_interface_descriptor(ptr, ctx->interface, 0, 1, USB_CLASS_HID, ctx->subclass, ctx->protocol,
                                     class_ctx->interface_string_index);

    uint8_t *hid_desc_ptr = ptr;
    ptr = hid_build_hid_descriptor(ptr, ctx->report_descriptor_len);
    memcpy(ctx->hid_descriptor, hid_desc_ptr, USB_HID_DESC_LEN);

    ptr = build_endpoint_descriptor(ptr, ctx->in_ep, USB_ENDP_TYPE_INTR, ep_mps, ctx->interval, speed, 0, 0, 0);

    return (uint32_t)(ptr - buffer);
}

static xRETURN_t hid_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    xRETURN_t status = hid_call_bus_event_callback(class_ctx, event);
    xRETURN_t dcd_status = xRETURN_OK;

    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_SPEED_CHANGE_RECEIVED)
    {
        dcd_status = hid_init_in_endpoint(class_ctx, ctx);
    }
    else if (event == USB_DCD_DISCONNECT_RECEIVED || event == USB_DCD_RESET_RECEIVED)
    {
        dcd_status = hid_deinit_in_endpoint(class_ctx, ctx);
    }

    if (status == xRETURN_OK)
    {
        status = dcd_status;
    }

    return status;
}

static xRETURN_t hid_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (hid_request_targets_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if ((req->bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD)
    {
        return hid_standard_control_in_request(ctx, response);
    }

    if ((req->bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS)
    {
        return hid_class_control_in_request(class_ctx, ctx, response);
    }

    return xRETURN_OK;
}

static xRETURN_t hid_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (hid_request_targets_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if ((req->bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD)
    {
        return hid_standard_control_out_request(ctx);
    }

    if ((req->bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS)
    {
        return hid_class_control_out_request(class_ctx, ctx, control_data, length);
    }

    return xRETURN_OK;
}

static xRETURN_t hid_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_HID_Callbacks_t *callbacks = xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_HID_Callbacks_t);
    if ((callbacks != NULL) && (callbacks->on_data_received != NULL))
    {
        return callbacks->on_data_received(class_ctx, ep_addr, data, length);
    }
    return xRETURN_OK;
}

static xRETURN_t hid_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_HID_Callbacks_t *callbacks = xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_HID_Callbacks_t);
    if ((callbacks != NULL) && (callbacks->on_transmit_complete != NULL))
    {
        return callbacks->on_transmit_complete(class_ctx, ep_addr, data, length);
    }
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xUSBD_Class_Driver_t *xUSBD_HID_Class(void)
{
    static xUSBD_Class_Driver_t s_driver = {
        .init_instance = hid_init_instance,
        .build_descriptor = hid_build_descriptor,
        .bus_event = hid_bus_event,
        .control_in_request = hid_control_in_request,
        .control_out_request = hid_control_out_request,
        .data_received = hid_data_received,
        .transmit_complete = hid_transmit_complete,
    };
    return &s_driver;
}

xRETURN_t xUSBD_HID_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Callbacks_t *callbacks)
{
    return xUSBD_Class_Set_Callbacks(class_ctx, callbacks);
}

xRETURN_t xUSBD_HID_Send_Report(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_HID_Context_t *ctx = (xUSBD_HID_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Send(class_ctx, ctx->in_ep, data, length, false);
}
// EOF /////////////////////////////////////////////////////////////////////////////
