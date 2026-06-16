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

// @file xusbd_cdc.c
// @brief xUSB Device Communications Class (CDC) driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xusbd_config.h"
#include "xusbd_return.h"
#include "xusbd_class.h"
#include "xusbd_cdc.h"
#include "xassert.h"

#include "xusbd_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////
// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t init_instance(xUSBD_Class_Context_t *class_ctx);
static uint32_t build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
static xRETURN_t bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
static xRETURN_t data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xUSBD_CDC_Callbacks_t *cdc_callbacks(xUSBD_Class_Context_t *class_ctx);

static inline uint8_t *build_header_desc(uint8_t *p, uint16_t bcd_cdc);
static inline uint8_t *build_call_mgmt_desc(uint8_t *p, uint8_t bm_capabilities, uint8_t data_interface);
static inline uint8_t *build_acm_desc(uint8_t *p, uint8_t bm_capabilities);
static inline uint8_t *build_union_desc(uint8_t *p, uint8_t master, uint8_t slave);
static inline uint8_t *build_ethernet_desc(uint8_t *p, uint8_t i_mac_address, uint16_t w_max_segment_size);
static inline uint8_t *build_ncm_desc(uint8_t *p, uint16_t bcd_ncm_version);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xUSBD_CDC_Callbacks_t *cdc_callbacks(xUSBD_Class_Context_t *class_ctx)
{
    return xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_CDC_Callbacks_t);
}

static xRETURN_t init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    xRETURN_t s = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->cmd_interface);
    if (s != xRETURN_OK)
    {
        return s;
    }
    s = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->data_interface);
    if (s != xRETURN_OK)
    {
        return s;
    }
    if (ctx->has_notification_ep)
    {
        s = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_IN, &ctx->cmd_ep);
        if (s != xRETURN_OK)
        {
            return s;
        }
    }
    s = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_OUT, &ctx->out_ep);
    if (s != xRETURN_OK)
    {
        return s;
    }
    s = xUSBD_Class_Allocate_Endpoint(class_ctx, USB_EP_DIR_IN, &ctx->in_ep);
    if (s != xRETURN_OK)
    {
        return s;
    }
    return xRETURN_OK;
}

static inline uint8_t *build_header_desc(uint8_t *p, uint16_t bcd_cdc)
{
    USB_CDC_Header_Functional_Descriptor_t *d = (USB_CDC_Header_Functional_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_HEADER_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x00U;
    d->bcdCDC = xCPU_TO_LE16(bcd_cdc);
    return p + USB_CDC_HEADER_FUNC_DESC_LEN;
}

static inline uint8_t *build_call_mgmt_desc(uint8_t *p, uint8_t bm_capabilities, uint8_t data_interface)
{
    USB_CDC_Call_Management_Functional_Descriptor_t *d = (USB_CDC_Call_Management_Functional_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_CALL_MGMT_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x01U;
    d->bmCapabilities = bm_capabilities;
    d->bDataInterface = data_interface;
    return p + USB_CDC_CALL_MGMT_FUNC_DESC_LEN;
}

static inline uint8_t *build_acm_desc(uint8_t *p, uint8_t bm_capabilities)
{
    USB_CDC_Abstract_Control_Management_Descriptor_t *d = (USB_CDC_Abstract_Control_Management_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_ACM_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x02U;
    d->bmCapabilities = bm_capabilities;
    return p + USB_CDC_ACM_FUNC_DESC_LEN;
}

static inline uint8_t *build_union_desc(uint8_t *p, uint8_t master, uint8_t slave)
{
    USB_CDC_Union_Functional_Descriptor_t *d = (USB_CDC_Union_Functional_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_UNION_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x06U;
    d->bMasterInterface = master;
    d->bSlaveInterface0 = slave;
    return p + USB_CDC_UNION_FUNC_DESC_LEN;
}

static inline uint8_t *build_ethernet_desc(uint8_t *p, uint8_t i_mac_address, uint16_t w_max_segment_size)
{
    USB_CDC_Ethernet_Functional_Descriptor_t *d = (USB_CDC_Ethernet_Functional_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_ETHERNET_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x0FU;
    d->iMACAddress = i_mac_address;
    d->bmEthernetStatistics = 0x00000000U;
    d->wMaxSegmentSize = xCPU_TO_LE16(w_max_segment_size);
    d->wNumberMCFilters = xCPU_TO_LE16(0U);
    d->bNumberPowerFilters = 0x00U;
    return p + USB_CDC_ETHERNET_FUNC_DESC_LEN;
}

static inline uint8_t *build_ncm_desc(uint8_t *p, uint16_t bcd_ncm_version)
{
    USB_CDC_NCM_Functional_Descriptor_t *d = (USB_CDC_NCM_Functional_Descriptor_t *)p;
    d->bFunctionLength = USB_CDC_NCM_FUNC_DESC_LEN;
    d->bDescriptorType = USB_DESC_TYPE_CS_INTERFACE;
    d->bDescriptorSubtype = 0x1AU;
    d->bcdNcmVersion = xCPU_TO_LE16(bcd_ncm_version);
    d->bmNetworkCapabilities = 0x00U;
    return p + USB_CDC_NCM_FUNC_DESC_LEN;
}

static uint32_t build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    uint16_t bulk_mps = ep_max_mps(speed, USB_ENDP_TYPE_BULK);
    (void)xUSBD_Class_Set_EP_MPS(class_ctx, bulk_mps);
    uint8_t *ptr = buffer;

    ptr = build_iad_descriptor(ptr, ctx->cmd_interface, 2U, USB_CLASS_COMMUNICATION, ctx->subclass, ctx->protocol,
                               class_ctx->interface_string_index);

    ptr = build_interface_descriptor(ptr, ctx->cmd_interface, 0U, 1U, USB_CLASS_COMMUNICATION, ctx->subclass, ctx->protocol,
                                     class_ctx->interface_string_index);

    // Build subclass functional descriptors dynamically
    if (ctx->subclass == 0x02U) // ACM
    {
        ptr = build_header_desc(ptr, 0x0110U);
        ptr = build_call_mgmt_desc(ptr, 0x00U, ctx->data_interface);
        ptr = build_acm_desc(ptr, 0x02U);
        ptr = build_union_desc(ptr, ctx->cmd_interface, ctx->data_interface);
    }
    else if (ctx->subclass == 0x06U) // ECM
    {
        ptr = build_header_desc(ptr, 0x0110U);
        ptr = build_union_desc(ptr, ctx->cmd_interface, ctx->data_interface);
        ptr = build_ethernet_desc(ptr, class_ctx->interface_string_index, 1514U);
    }
    else if (ctx->subclass == 0x0DU) // NCM
    {
        ptr = build_header_desc(ptr, 0x0120U);
        ptr = build_union_desc(ptr, ctx->cmd_interface, ctx->data_interface);
        ptr = build_ethernet_desc(ptr, class_ctx->interface_string_index, 1514U);
        ptr = build_ncm_desc(ptr, 0x0100U);
    }

    if (ctx->has_notification_ep)
    {
        ptr = build_endpoint_descriptor(ptr, ctx->cmd_ep, USB_ENDP_TYPE_INTR, ctx->cmd_ep_mps, ctx->cmd_ep_interval, speed, 0U, 0U, 0U);
    }

    ptr = build_interface_descriptor(ptr, ctx->data_interface, 0U, 2U, 0x0AU, 0x00U, 0x00U, class_ctx->interface_string_index);
    ptr = build_endpoint_descriptor(ptr, ctx->out_ep, USB_ENDP_TYPE_BULK, bulk_mps, 0x00U, speed, 0U, 0U, 0U);
    ptr = build_endpoint_descriptor(ptr, ctx->in_ep, USB_ENDP_TYPE_BULK, bulk_mps, 0x00U, speed, 0U, 0U, 0U);

    return (uint32_t)(ptr - buffer);
}

static xRETURN_t bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    xRETURN_t status = xRETURN_OK;
    xRETURN_t dcd_status = xRETURN_OK;

    xUSBD_CDC_Callbacks_t *callbacks = cdc_callbacks(class_ctx);
    if (callbacks != NULL && callbacks->on_bus_event != NULL)
    {
        status = callbacks->on_bus_event(class_ctx, event);
    }

    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_SPEED_CHANGE_RECEIVED)
    {
        uint16_t ep_mps = 0U;
        dcd_status = xUSBD_Class_Get_EP_MPS(class_ctx, &ep_mps);
        if (dcd_status != xRETURN_OK)
        {
            return dcd_status;
        }

        dcd_status = xUSBD_Class_DCD_EP_Init(class_ctx, ctx->in_ep, USB_ENDP_TYPE_BULK, ep_mps);
        if (dcd_status == xRETURN_OK)
        {
            dcd_status = xUSBD_Class_DCD_EP_Init(class_ctx, ctx->out_ep, USB_ENDP_TYPE_BULK, ep_mps);
        }
        if (dcd_status == xRETURN_OK && ctx->has_notification_ep)
        {
            dcd_status = xUSBD_Class_DCD_EP_Init(class_ctx, ctx->cmd_ep, USB_ENDP_TYPE_INTR, ctx->cmd_ep_mps);
        }
    }
    else if (event == USB_DCD_DISCONNECT_RECEIVED || event == USB_DCD_RESET_RECEIVED)
    {
        dcd_status = xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->in_ep);
        if (dcd_status == xRETURN_OK)
        {
            dcd_status = xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->out_ep);
        }
        if (dcd_status == xRETURN_OK && ctx->has_notification_ep)
        {
            dcd_status = xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->cmd_ep);
        }
    }

    if (status == xRETURN_OK)
    {
        status = dcd_status;
    }
    return status;
}

static xRETURN_t control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_CDC_Callbacks_t *callbacks = cdc_callbacks(class_ctx);
    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    if (callbacks->on_control_in != NULL)
    {
        return callbacks->on_control_in(class_ctx, response);
    }

    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Callbacks_t *callbacks = cdc_callbacks(class_ctx);
    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    if (callbacks->on_control_out != NULL)
    {
        return callbacks->on_control_out(class_ctx, data, length);
    }

    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    if (ep_addr != ctx->out_ep)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    xUSBD_CDC_Callbacks_t *callbacks = cdc_callbacks(class_ctx);
    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    if (callbacks->on_data_received != NULL)
    {
        return callbacks->on_data_received(class_ctx, ep_addr, data, length);
    }

    return xRETURN_OK;
}

static xRETURN_t transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    bool cmd_ep_match = ctx->has_notification_ep && (ep_addr == ctx->cmd_ep);
    if (ep_addr != ctx->in_ep && !cmd_ep_match)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    xUSBD_CDC_Callbacks_t *callbacks = cdc_callbacks(class_ctx);
    if (callbacks == NULL)
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    if (callbacks->on_transmit_complete != NULL)
    {
        return callbacks->on_transmit_complete(class_ctx, ep_addr, data, length);
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
xRETURN_t xUSBD_CDC_Prepare_To_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Receive(class_ctx, ctx->out_ep, data, length);
}

xRETURN_t xUSBD_CDC_Transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    uint16_t ep_mps = 0U;
    xRETURN_t status = xUSBD_Class_Get_EP_MPS(class_ctx, &ep_mps);
    if (status != xRETURN_OK)
    {
        return status;
    }
    uint8_t is_zlp_required = ((length % ep_mps) == 0U) && (length > 0U);
    return xUSBD_Class_DCD_EP_Send(class_ctx, ctx->in_ep, data, length, is_zlp_required);
}

xRETURN_t xUSBD_CDC_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_Callbacks_t *callbacks)
{
    return xUSBD_Class_Set_Callbacks(class_ctx, callbacks);
}

xRETURN_t xUSBD_CDC_Send_Notification(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    xASSERT(ctx->has_notification_ep, "Send_Notification requires has_notification_ep = true");
    return xUSBD_Class_DCD_EP_Send(class_ctx, ctx->cmd_ep, data, length, false);
}

xUSBD_Class_Driver_t *xUSBD_CDC_Class(void)
{
    static xUSBD_Class_Driver_t s_driver = {
        .init_instance = init_instance,
        .build_descriptor = build_descriptor,
        .bus_event = bus_event,
        .control_in_request = control_in_request,
        .control_out_request = control_out_request,
        .data_received = data_received,
        .transmit_complete = transmit_complete,
    };
    return &s_driver;
}
// EOF /////////////////////////////////////////////////////////////////////////////
