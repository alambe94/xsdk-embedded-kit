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

// @file xusbd_dfu.c
// @brief xUSB Device Firmware Upgrade (DFU) class driver implementation.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "string.h"
#include "xusbd_return.h"
#include "xusbd_class.h"
#include "xusbd_dfu.h"
#include "xassert.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
xSTATIC_ASSERT(sizeof(USB_DFU_Functional_Descriptor_t) == USB_DFU_FUNC_DESC_LEN, "USB DFU functional descriptor size changed");
xSTATIC_ASSERT(sizeof(USB_DFU_Descriptor_t) == xUSBD_DFU_DESC_SIZE, "xUSBD DFU descriptor size changed");
// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xUSBD_DFU_Callbacks_t *dfu_get_callbacks(xUSBD_DFU_Context_t *ctx);
static bool dfu_request_matches_interface(const xUSBD_DFU_Context_t *ctx);
static xRETURN_t dfu_control_response_buffer(xUSBD_Class_Context_t *class_ctx, uint8_t **buffer);
static void dfu_getstatus_build_response(xUSBD_DFU_Context_t *ctx, uint8_t *control_buffer);
static xRETURN_t dfu_upload_request_process(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_Response_t *response);
static xRETURN_t dfu_process_write_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_DFU_Callbacks_t *callbacks);
static xRETURN_t dfu_process_manifest_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_DFU_Callbacks_t *callbacks);
static void dfu_process_detach_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Callbacks_t *callbacks);
static void dfu_build_status(xUSBD_DFU_Context_t *ctx, uint8_t *buffer, uint32_t poll_ms);
static void dfu_reset_state(xUSBD_DFU_Context_t *ctx);
static xRETURN_t dfu_runtime_init_instance(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t dfu_mode_init_instance(xUSBD_Class_Context_t *class_ctx);
static uint32_t dfu_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
static xRETURN_t dfu_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t dfu_control_transfer_complete(xUSBD_Class_Context_t *class_ctx, USB_Setup_Request_t *request);
static xRETURN_t dfu_runtime_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t dfu_runtime_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
static xRETURN_t dfu_mode_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t dfu_mode_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
//

static xUSBD_DFU_Callbacks_t *dfu_get_callbacks(xUSBD_DFU_Context_t *ctx)
{
    return xUSBD_CLASS_CALLBACKS(&ctx->class_ctx, xUSBD_DFU_Callbacks_t);
}

static bool dfu_request_matches_interface(const xUSBD_DFU_Context_t *ctx)
{
    return xU16_LOW_BYTE(ctx->class_ctx.device_ctx->request.wIndex) == ctx->interface;
}

static xRETURN_t dfu_control_response_buffer(xUSBD_Class_Context_t *class_ctx, uint8_t **buffer)
{
    uint32_t buffer_length = 0U;
    xRETURN_t status = xUSBD_Class_Get_Control_Buffer(class_ctx, buffer, &buffer_length);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (buffer_length < sizeof(USB_DFU_Status_Response_t))
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }

    return xRETURN_OK;
}

// Fill the 6-byte GETSTATUS wire response into buffer; bState is taken from ctx.
// Call site overrides bState/bwPollTimeout where the spec requires a virtual
// state (dfuDNBUSY, dfuMANIFEST) that is not stored in ctx->state.
static void dfu_build_status(xUSBD_DFU_Context_t *ctx, uint8_t *buffer, uint32_t poll_ms)
{
    USB_DFU_Status_Response_t *response = (USB_DFU_Status_Response_t *)buffer;
    response->bStatus = (uint8_t)ctx->dfu_status;
    response->bwPollTimeout[0] = (uint8_t)(poll_ms & 0xFFU);
    response->bwPollTimeout[1] = (uint8_t)((poll_ms >> 8U) & 0xFFU);
    response->bwPollTimeout[2] = (uint8_t)((poll_ms >> 16U) & 0xFFU);
    response->bState = (uint8_t)ctx->state;
    response->iString = 0U;
}

static void dfu_reset_state(xUSBD_DFU_Context_t *ctx)
{
    ctx->state = (ctx->protocol == USB_DFU_PROTOCOL_DFU) ? xUSBD_DFU_STATE_DFU_IDLE : xUSBD_DFU_STATE_APP_IDLE;
    ctx->dfu_status = xUSBD_DFU_STATUS_OK;
    ctx->pending_op = xUSBD_DFU_PENDING_NONE;
    ctx->block_num = 0;
    ctx->dnload_length = 0;
}

static xRETURN_t dfu_runtime_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    xRETURN_t status = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->interface);
    if (status != xRETURN_OK)
    {
        return status;
    }
    ctx->protocol = USB_DFU_PROTOCOL_RUNTIME;
    ctx->transfer_size = xUSBD_DFU_TRANSFER_SIZE;
    dfu_reset_state(ctx);
    return xRETURN_OK;
}

static xRETURN_t dfu_mode_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    xRETURN_t status = xUSBD_Class_Allocate_Interface(class_ctx, &ctx->interface);
    if (status != xRETURN_OK)
    {
        return status;
    }
    ctx->protocol = USB_DFU_PROTOCOL_DFU;
    ctx->transfer_size = xUSBD_DFU_TRANSFER_SIZE;
    dfu_reset_state(ctx);
    return xRETURN_OK;
}

static uint32_t dfu_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    (void)speed;
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    USB_DFU_Descriptor_t *descriptor = (USB_DFU_Descriptor_t *)buffer;

    // Runtime: advertise all capabilities and declare self-detach support.
    // DFU mode: do not set WILL_DETACH (host resets the bus after manifest).
    uint8_t bm_attr = (ctx->protocol == USB_DFU_PROTOCOL_RUNTIME)
                          ? (USB_DFU_ATTR_WILL_DETACH | USB_DFU_ATTR_CAN_DNLOAD | USB_DFU_ATTR_CAN_UPLOAD | USB_DFU_ATTR_MANIFEST_TOLERANT)
                          : (USB_DFU_ATTR_CAN_DNLOAD | USB_DFU_ATTR_CAN_UPLOAD | USB_DFU_ATTR_MANIFEST_TOLERANT);

    build_interface_descriptor((uint8_t *)&descriptor->interface_descriptor, ctx->interface, 0x00, 0x00, USB_CLASS_APPLICATION_SPECIFIC,
                               USB_DFU_SUBCLASS, ctx->protocol, class_ctx->interface_string_index);

    descriptor->dfu_functional_descriptor.bLength = USB_DFU_FUNC_DESC_LEN;
    descriptor->dfu_functional_descriptor.bDescriptorType = USB_DFU_FUNC_DESC_TYPE;
    descriptor->dfu_functional_descriptor.bmAttributes = bm_attr;
    descriptor->dfu_functional_descriptor.wDetachTimeout = xCPU_TO_LE16(1000U);
    descriptor->dfu_functional_descriptor.wTransferSize = xCPU_TO_LE16(ctx->transfer_size);
    descriptor->dfu_functional_descriptor.bcdDFUVersion = xCPU_TO_LE16(0x0110U);

    return xUSBD_DFU_DESC_SIZE;
}

static xRETURN_t dfu_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    xUSBD_DFU_Callbacks_t *callbacks = dfu_get_callbacks(ctx);

    if ((callbacks != NULL) && (callbacks->on_bus_event != NULL))
    {
        (void)callbacks->on_bus_event(class_ctx, event);
    }

    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_DISCONNECT_RECEIVED)
    {
        dfu_reset_state(ctx);
    }
    else if (event == USB_DCD_RESET_RECEIVED && ctx->state == xUSBD_DFU_STATE_DFU_MANIFEST_WAIT_RESET)
    {
        // Bus reset received while waiting for reset after manifest trigger reboot.
        ctx->pending_op = xUSBD_DFU_PENDING_DETACH;
    }

    return xRETURN_OK;
}

//
// Called after the STATUS phase of any EP0 transfer finishes, for both IN
// (host ZLP acks our response) and OUT (our ZLP acks host data).
// This is the deferred-action trigger the DFU spec requires.

static xRETURN_t dfu_control_transfer_complete(xUSBD_Class_Context_t *class_ctx, USB_Setup_Request_t *request)
{
    (void)request; // request == &class_ctx->device_ctx->request; use context directly
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    // Ignore requests not addressed to our interface or not class-type.
    if (dfu_request_matches_interface(ctx) == false)
    {
        return xRETURN_OK;
    }
    if ((req->bRequestType & USB_REQ_TYPE_MASK) != USB_REQ_TYPE_CLASS)
    {
        return xRETURN_OK;
    }

    switch (req->bRequest)
    {
    case USB_DFU_REQ_DETACH:
        ctx->pending_op = xUSBD_DFU_PENDING_DETACH;
        break;

    case USB_DFU_REQ_DNLOAD:
        if (ctx->dnload_length > 0)
        {
            ctx->pending_op = xUSBD_DFU_PENDING_WRITE;
        }
        else
        {
            ctx->pending_op = xUSBD_DFU_PENDING_MANIFEST;
        }
        break;

    case USB_DFU_REQ_GETSTATUS:
        // Transition to the stable state only when there is no pending work.
        // If pending_op is still set, the op is not done; stay in SYNC and let
        // the host keep polling.
        if (ctx->state == xUSBD_DFU_STATE_DFU_DNLOAD_SYNC && ctx->pending_op == xUSBD_DFU_PENDING_NONE)
        {
            ctx->state = xUSBD_DFU_STATE_DFU_DNLOAD_IDLE;
        }
        else if (ctx->state == xUSBD_DFU_STATE_DFU_MANIFEST_SYNC && ctx->pending_op == xUSBD_DFU_PENDING_NONE)
        {
            ctx->state = xUSBD_DFU_STATE_DFU_MANIFEST_WAIT_RESET;
        }
        break;

    default:
        break;
    }

    return xRETURN_OK;
}

static xRETURN_t dfu_runtime_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    uint8_t *control_buffer = NULL;
    xRETURN_t status = dfu_control_response_buffer(class_ctx, &control_buffer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (dfu_request_matches_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    switch (class_ctx->device_ctx->request.bRequest)
    {
    case USB_DFU_REQ_GETSTATUS:
        dfu_build_status(ctx, control_buffer, 0U);
        response->data = control_buffer;
        response->length = sizeof(USB_DFU_Status_Response_t);
        return xRETURN_OK;

    case USB_DFU_REQ_GETSTATE:
        control_buffer[0] = (uint8_t)ctx->state;
        response->data = control_buffer;
        response->length = 1U;
        return xRETURN_OK;

    default:
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }
}

static xRETURN_t dfu_runtime_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;

    if (dfu_request_matches_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    switch (class_ctx->device_ctx->request.bRequest)
    {
    case USB_DFU_REQ_DETACH:
        if (ctx->state == xUSBD_DFU_STATE_APP_IDLE)
        {
            ctx->state = xUSBD_DFU_STATE_APP_DETACH;
            // pending_op = DETACH is set in CTF_Complete after status ZLP is sent.
            return xRETURN_OK;
        }
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;

    case USB_DFU_REQ_ABORT:
        ctx->state = xUSBD_DFU_STATE_APP_IDLE;
        return xRETURN_OK;

    default:
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }
}

static void dfu_getstatus_build_response(xUSBD_DFU_Context_t *ctx, uint8_t *control_buffer)
{
    USB_DFU_Status_Response_t *response = (USB_DFU_Status_Response_t *)control_buffer;

    switch (ctx->state)
    {
    case xUSBD_DFU_STATE_DFU_DNLOAD_SYNC:
        if (ctx->pending_op == xUSBD_DFU_PENDING_WRITE)
        {
            dfu_build_status(ctx, control_buffer, xUSBD_DFU_POLL_TIMEOUT_MS);
            response->bState = xUSBD_DFU_STATE_DFU_DNBUSY;
        }
        else
        {
            dfu_build_status(ctx, control_buffer, 0U);
            response->bState = xUSBD_DFU_STATE_DFU_DNLOAD_IDLE;
        }
        break;
    case xUSBD_DFU_STATE_DFU_MANIFEST_SYNC:
        if (ctx->pending_op == xUSBD_DFU_PENDING_MANIFEST)
        {
            dfu_build_status(ctx, control_buffer, xUSBD_DFU_POLL_TIMEOUT_MS);
            response->bState = xUSBD_DFU_STATE_DFU_MANIFEST;
        }
        else
        {
            dfu_build_status(ctx, control_buffer, 0U);
            response->bState = xUSBD_DFU_STATE_DFU_MANIFEST_WAIT_RESET;
        }
        break;
    default:
        dfu_build_status(ctx, control_buffer, 0U);
        break;
    }
}

static xRETURN_t dfu_upload_request_process(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_Response_t *response)
{
    xUSBD_DFU_Callbacks_t *callbacks = dfu_get_callbacks(ctx);
    const USB_Setup_Request_t *req = &class_ctx->device_ctx->request;

    if (ctx->state != xUSBD_DFU_STATE_DFU_IDLE && ctx->state != xUSBD_DFU_STATE_DFU_UPLOAD_IDLE)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if ((callbacks == NULL) || (callbacks->on_io_control == NULL))
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    uint16_t block = req->wValue;
    uint8_t *upload_ptr = NULL;
    uint32_t upload_length = ctx->transfer_size;

    xRETURN_t callback_status =
        callbacks->on_io_control(class_ctx, xUSBD_DFU_IO_CMD_READ_BLOCK, &block, sizeof(block), (void **)&upload_ptr, &upload_length);
    if ((callback_status != xRETURN_OK) || (upload_ptr == NULL))
    {
        ctx->dfu_status = xUSBD_DFU_STATUS_ERR_FILE;
        ctx->state = xUSBD_DFU_STATE_DFU_ERROR;
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    response->data = upload_ptr;
    response->length = upload_length;
    ctx->block_num = block;

    ctx->state = (upload_length < req->wLength) ? xUSBD_DFU_STATE_DFU_IDLE : xUSBD_DFU_STATE_DFU_UPLOAD_IDLE;
    return xRETURN_OK;
}

static xRETURN_t dfu_mode_control_in_request(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    uint8_t *control_buffer = NULL;
    xRETURN_t status = dfu_control_response_buffer(class_ctx, &control_buffer);
    if (status != xRETURN_OK)
    {
        return status;
    }

    if (dfu_request_matches_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    switch (class_ctx->device_ctx->request.bRequest)
    {
    case USB_DFU_REQ_GETSTATUS:
        dfu_getstatus_build_response(ctx, control_buffer);
        response->data = control_buffer;
        response->length = sizeof(USB_DFU_Status_Response_t);
        return xRETURN_OK;
    case USB_DFU_REQ_GETSTATE:
        control_buffer[0] = (uint8_t)ctx->state;
        response->data = control_buffer;
        response->length = 1U;
        return xRETURN_OK;
    case USB_DFU_REQ_UPLOAD:
        return dfu_upload_request_process(class_ctx, ctx, response);
    default:
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }
}

static xRETURN_t dfu_mode_control_out_request(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;

    if (dfu_request_matches_interface(ctx) == false)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    switch (class_ctx->device_ctx->request.bRequest)
    {
    case USB_DFU_REQ_DNLOAD:
    {
        if (ctx->state != xUSBD_DFU_STATE_DFU_IDLE && ctx->state != xUSBD_DFU_STATE_DFU_DNLOAD_IDLE)
        {
            return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        }

        if (length > 0)
        {
            // Clamp to configured transfer size; copy out of the shared control buffer
            // before it is overwritten by the GETSTATUS response.
            if (length > ctx->transfer_size)
            {
                length = ctx->transfer_size;
            }

            ctx->block_num = class_ctx->device_ctx->request.wValue;
            ctx->dnload_length = length;
            memcpy(ctx->dnload_buffer, data, length);
            ctx->state = xUSBD_DFU_STATE_DFU_DNLOAD_SYNC;
            // pending_op = WRITE is set in CTF_Complete after the status ZLP fires.
        }
        else
        {
            // Zero-length DNLOAD = end of firmware image enter manifest phase.
            ctx->dnload_length = 0U;
            ctx->state = xUSBD_DFU_STATE_DFU_MANIFEST_SYNC;
            // pending_op = MANIFEST is set in CTF_Complete.
        }
        return xRETURN_OK;
    }

    case USB_DFU_REQ_CLRSTATUS:
        if (ctx->state == xUSBD_DFU_STATE_DFU_ERROR)
        {
            ctx->dfu_status = xUSBD_DFU_STATUS_OK;
            ctx->state = xUSBD_DFU_STATE_DFU_IDLE;
            ctx->pending_op = xUSBD_DFU_PENDING_NONE;
        }
        return xRETURN_OK;

    case USB_DFU_REQ_ABORT:
        switch (ctx->state)
        {
        case xUSBD_DFU_STATE_DFU_IDLE:
        case xUSBD_DFU_STATE_DFU_DNLOAD_SYNC:
        case xUSBD_DFU_STATE_DFU_DNLOAD_IDLE:
        case xUSBD_DFU_STATE_DFU_MANIFEST_SYNC:
        case xUSBD_DFU_STATE_DFU_UPLOAD_IDLE:
            ctx->state = xUSBD_DFU_STATE_DFU_IDLE;
            ctx->pending_op = xUSBD_DFU_PENDING_NONE;
            ctx->dnload_length = 0;
            return xRETURN_OK;
        default:
            return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
        }
        break;

    default:
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }
}

static xRETURN_t dfu_process_write_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_DFU_Callbacks_t *callbacks)
{
    xUSBD_DFU_Write_Block_t write_block;
    write_block.block_num = ctx->block_num;
    write_block.data = ctx->dnload_buffer;
    write_block.length = ctx->dnload_length;

    xRETURN_t callback_status =
        callbacks->on_io_control(class_ctx, xUSBD_DFU_IO_CMD_WRITE_BLOCK, &write_block, sizeof(write_block), NULL, NULL);
    if (callback_status != xRETURN_OK)
    {
        ctx->dfu_status = xUSBD_DFU_STATUS_ERR_WRITE;
        ctx->state = xUSBD_DFU_STATE_DFU_ERROR;
    }

    return xRETURN_OK;
}

static xRETURN_t dfu_process_manifest_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Context_t *ctx, xUSBD_DFU_Callbacks_t *callbacks)
{
    xRETURN_t callback_status = callbacks->on_io_control(class_ctx, xUSBD_DFU_IO_CMD_MANIFEST, NULL, 0U, NULL, NULL);
    if (callback_status != xRETURN_OK)
    {
        ctx->dfu_status = xUSBD_DFU_STATUS_ERR_VERIFY;
        ctx->state = xUSBD_DFU_STATE_DFU_ERROR;
    }

    return xRETURN_OK;
}

static void dfu_process_detach_pending(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Callbacks_t *callbacks)
{
    // Call does not return if it reboots the device.
    (void)callbacks->on_io_control(class_ctx, xUSBD_DFU_IO_CMD_DETACH, NULL, 0U, NULL, NULL);
}

xUSBD_Class_Driver_t *xUSBD_DFU_Runtime_Class(void)
{
    static xUSBD_Class_Driver_t driver = {
        .init_instance = dfu_runtime_init_instance,
        .build_descriptor = dfu_build_descriptor,
        .bus_event = dfu_bus_event,
        .control_in_request = dfu_runtime_control_in_request,
        .control_out_request = dfu_runtime_control_out_request,
        .control_transfer_complete = dfu_control_transfer_complete,
    };
    return &driver;
}

xUSBD_Class_Driver_t *xUSBD_DFU_Mode_Class(void)
{
    static xUSBD_Class_Driver_t driver = {
        .init_instance = dfu_mode_init_instance,
        .build_descriptor = dfu_build_descriptor,
        .bus_event = dfu_bus_event,
        .control_in_request = dfu_mode_control_in_request,
        .control_out_request = dfu_mode_control_out_request,
        .control_transfer_complete = dfu_control_transfer_complete,
    };
    return &driver;
}

xRETURN_t xUSBD_DFU_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_DFU_Callbacks_t *callbacks)
{
    return xUSBD_Class_Set_Callbacks(class_ctx, callbacks);
}

xRETURN_t xUSBD_DFU_Set_Transfer_Size(xUSBD_Class_Context_t *class_ctx, uint16_t size)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    if (size == 0 || size > xUSBD_DFU_TRANSFER_SIZE)
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }
    ctx->transfer_size = size;
    return xRETURN_OK;
}

xUSBD_DFU_Pending_Op_t xUSBD_DFU_Get_Pending_Op(xUSBD_Class_Context_t *class_ctx)
{
    return ((xUSBD_DFU_Context_t *)class_ctx)->pending_op;
}

xRETURN_t xUSBD_DFU_Process_Pending_Op(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_DFU_Context_t *ctx = (xUSBD_DFU_Context_t *)class_ctx;
    xUSBD_DFU_Callbacks_t *callbacks = dfu_get_callbacks(ctx);

    if ((callbacks == NULL) || (callbacks->on_io_control == NULL))
    {
        return xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    }

    // Snapshot and clear before executing so ISR-level re-entry sets a fresh op.
    xUSBD_DFU_Pending_Op_t op = ctx->pending_op;
    ctx->pending_op = xUSBD_DFU_PENDING_NONE;

    switch (op)
    {
    case xUSBD_DFU_PENDING_WRITE:
        (void)dfu_process_write_pending(class_ctx, ctx, callbacks);
        break;

    case xUSBD_DFU_PENDING_MANIFEST:
        (void)dfu_process_manifest_pending(class_ctx, ctx, callbacks);
        break;

    case xUSBD_DFU_PENDING_DETACH:
        dfu_process_detach_pending(class_ctx, callbacks);
        break;

    default:
        break;
    }

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
