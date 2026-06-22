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

// @file xusbd_win.c
// @brief xUSB Windows USB (WinUSB) driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "string.h"
#include "xusbd_return.h"
#include "xusbd_class.h"
#include "xusbd_win.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

static uint8_t winusb_device_interface_guid_utf16[] = {
    '{', 0, '8', 0, 'D', 0, '5', 0, 'B', 0, '9', 0, 'B', 0, '7', 0,
    '9', 0, '-', 0, '1', 0, 'F', 0, '4', 0, '8', 0, '-', 0, '4', 0,
    'C', 0, '8', 0, 'A', 0, '-', 0, '9', 0, 'E', 0, '2', 0, 'D', 0,
    '-', 0, '0', 0, 'D', 0, '5', 0, 'A', 0, '8', 0, 'F', 0, '6', 0,
    'A', 0, '1', 0, '2', 0, '0', 0, '9', 0, '}', 0, 0, 0, 0, 0,
};

static xUSBD_MOS_Property_t winusb_mos_props[] = {
    {0x0007U, "DeviceInterfaceGUIDs", winusb_device_interface_guid_utf16, sizeof(winusb_device_interface_guid_utf16)},
    {0},
};

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t win_init_instance(xUSBD_Class_Context_t *class_ctx);
static uint32_t win_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
static xRETURN_t win_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t win_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
static xRETURN_t win_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t win_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t win_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
//

static xUSBD_WIN_Callbacks_t *win_callbacks(xUSBD_Class_Context_t *class_ctx)
{
    return xUSBD_CLASS_CALLBACKS(class_ctx, xUSBD_WIN_Callbacks_t);
}

static xRETURN_t win_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;

    // Tell the MS OS 2.0 descriptor builder to emit CompatibleID = "WINUSB" for
    // this function so Windows automatically loads WinUSB.sys without a .inf file.
    class_ctx->ms_compatible_id = "WINUSB";
    class_ctx->mos_props = winusb_mos_props;

    xRETURN_t s = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->interface);
    if (s != xRETURN_OK)
    {
        return s;
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

static uint32_t win_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;
    uint16_t ep_mps = ep_max_mps(speed, USB_ENDP_TYPE_BULK);
    (void)xUSBD_Class_Set_EP_MPS(class_ctx, ep_mps);
    uint8_t *ptr = buffer;

    ptr = build_interface_descriptor(ptr, ctx->interface, 0, 2, USB_CLASS_VENDOR, 0x00, 0x00, class_ctx->interface_string_index);

    ptr = build_endpoint_descriptor(ptr, ctx->out_ep, USB_ENDP_TYPE_BULK, ep_mps, 0x00, speed, 0, 0, 0);
    ptr = build_endpoint_descriptor(ptr, ctx->in_ep, USB_ENDP_TYPE_BULK, ep_mps, 0x00, speed, 0, 0, 0);

    return (uint32_t)(ptr - buffer);
}

static xRETURN_t win_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    (void)response;
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (req->bRequest != xUSBD_WINUSB_VENDOR_CODE && xU16_LOW_BYTE(req->wIndex) != ctx->interface)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    // Windows MOS 1.0 (WinUSB fallback if needed) or other custom vendor control requests.
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t win_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    (void)class_ctx;
    (void)control_data;
    (void)length;
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

xRETURN_t xUSBD_WIN_Prepare_To_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Receive(class_ctx, ctx->out_ep, data, length);
}

xRETURN_t xUSBD_WIN_Transmit(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;
    return xUSBD_Class_DCD_EP_Send(class_ctx, ctx->in_ep, data, length, is_zlp_required);
}

static xRETURN_t win_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;

    if (ep_addr == ctx->out_ep)
    {
        xUSBD_WIN_Callbacks_t *callbacks = win_callbacks(class_ctx);
        if ((callbacks != NULL) && (callbacks->on_data_received != NULL))
        {
            return callbacks->on_data_received(class_ctx, ep_addr, data, length);
        }
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t win_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_WIN_Callbacks_t *callbacks = win_callbacks(class_ctx);
    if ((callbacks != NULL) && (callbacks->on_transmit_complete != NULL))
    {
        return callbacks->on_transmit_complete(class_ctx, ep_addr, data, length);
    }
    return xRETURN_OK;
}

static xRETURN_t win_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_WIN_Context_t *ctx = (xUSBD_WIN_Context_t *)class_ctx;
    xRETURN_t status = xRETURN_OK;
    xRETURN_t dcd_status = xRETURN_OK;
    xUSBD_WIN_Callbacks_t *callbacks = win_callbacks(class_ctx);

    if ((callbacks != NULL) && (callbacks->on_bus_event != NULL))
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
    }
    else if (event == USB_DCD_DISCONNECT_RECEIVED || event == USB_DCD_RESET_RECEIVED)
    {
        dcd_status = xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->in_ep);
        if (dcd_status == xRETURN_OK)
        {
            dcd_status = xUSBD_Class_DCD_EP_Deinit(class_ctx, ctx->out_ep);
        }
    }

    if (status == xRETURN_OK)
    {
        status = dcd_status;
    }
    return status;
}

xRETURN_t xUSBD_WIN_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_WIN_Callbacks_t *callbacks)
{
    return xUSBD_Class_Set_Callbacks(class_ctx, callbacks);
}

xUSBD_Class_Driver_t *xUSBD_WIN_Class(void)
{
    static xUSBD_Class_Driver_t s_driver = {
        .init_instance = win_init_instance,
        .build_descriptor = win_build_descriptor,
        .bus_event = win_bus_event,
        .control_in_request = win_control_in_request,
        .control_out_request = win_control_out_request,
        .data_received = win_data_received,
        .transmit_complete = win_transmit_complete,
    };
    return &s_driver;
}
// EOF /////////////////////////////////////////////////////////////////////////////
