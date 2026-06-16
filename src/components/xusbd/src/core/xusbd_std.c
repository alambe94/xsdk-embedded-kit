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

// @file xusbd_std.c
// @brief USB device standard request handler and DCD event dispatcher.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbd_std.h"
#include "xusbd_config.h"
#include "xusbd_class.h"
#include "xusbd_return.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
// TYPES //////////////////////////////////////////////////////////////////////////
// xUSBD_Control_Phase_t is declared in xusbd_std.h (shared with xusbd_core.c).

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

static inline uint16_t ep0_mps(USB_Speed_t speed)
{
    return (speed == USB_SPEED_SUPER) ? 512U : (speed == USB_SPEED_LOW) ? 8U : 64U;
}

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t setup_request_dispatch(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t setup_data_stage_start(xUSBD_Device_Context_t *device_ctx, uint8_t *setup_data, uint32_t setup_data_length);
static xRETURN_t standard_request_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_get_descriptor_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_get_string_descriptor_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_get_status_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_clear_feature_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_set_feature_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_set_address_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_get_configuration_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_set_configuration_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_get_interface_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t standard_set_interface_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_set_sel_process(xUSBD_Device_Context_t *device_ctx, uint32_t *setup_data_length);
static xRETURN_t standard_set_isoch_delay_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t standard_out_data_process(xUSBD_Device_Context_t *device_ctx, uint8_t *data, uint32_t length);
static xRETURN_t string_to_string_descriptor(uint8_t *string, uint8_t *descriptor);
static xRETURN_t status_set_feature_complete_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t status_set_address_complete_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t status_set_configuration_complete_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t status_phase_complete_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t control_transfer_complete_process(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t ep0_out_data_received_process(xUSBD_Device_Context_t *device_ctx, uint32_t length);
static inline xRETURN_t ep0_stall(xUSBD_Device_Context_t *device_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
//

static xRETURN_t string_to_string_descriptor(uint8_t *string, uint8_t *descriptor)
{
    uint8_t index = 0U;

    if (string == NULL || descriptor == NULL)
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_NULL_POINTER, "NULL pointer exception");
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint8_t string_length = (uint8_t)strlen((char *)string);

    if (string_length > (xUSBD_MAX_EP0_DATA_SIZE - 2) / 2)
    {
        string_length = (xUSBD_MAX_EP0_DATA_SIZE - 2) / 2;
        xUSBD_LOG(xRETURN_xWRN_xUSBD_STRING_TRUNCATED, "String truncated");
    }

    descriptor[index++] = 2U + (2U * string_length);
    descriptor[index++] = USB_DESC_TYPE_STRING;

    while (string_length--)
    {
        descriptor[index++] = *string++;
        descriptor[index++] = 0U;
    }

    return xRETURN_OK;
}

static inline xRETURN_t ep0_stall(xUSBD_Device_Context_t *device_ctx)
{
    xASSERT(device_ctx != NULL, "device_ctx is NULL");
    xRETURN_t status = xUSBD_DCD_EP_Stall(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x80U);
    if (status == xRETURN_OK)
    {
        xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_STALL, 0x80U);
    }
    return status;
}

static xRETURN_t setup_request_dispatch(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    uint16_t w_length = device_ctx->request.wLength;
    uint8_t request_type = device_ctx->request.bRequestType & USB_REQ_TYPE_MASK;

    if (request_type == USB_REQ_TYPE_STANDARD)
    {
        return standard_request_process(device_ctx, response);
    }

    if (request_type == USB_REQ_TYPE_CLASS || request_type == USB_REQ_TYPE_VENDOR)
    {
        if (device_ctx->request.bRequestType & USB_REQ_TYPE_IN)
        {
            return xUSBD_Class_In_Request_Process(device_ctx, response);
        }

        if (w_length > 0U)
        {
            // OUT with data stage: class handler runs after EP0 OUT data arrives.
            response->length = w_length;
            return xRETURN_OK;
        }

        return xUSBD_Class_Out_Request_Process(device_ctx, device_ctx->control_data, 0x00);
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t setup_data_stage_start(xUSBD_Device_Context_t *device_ctx, uint8_t *setup_data, uint32_t setup_data_length)
{
    uint16_t w_length = device_ctx->request.wLength;
    setup_data_length = MIN(w_length, setup_data_length);

    if (device_ctx->request.bRequestType & USB_REQ_TYPE_IN)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_IN;
        xRETURN_t status = xUSBD_DCD_EP_Send(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x80U, setup_data, setup_data_length,
                                             (setup_data_length % ep0_mps(device_ctx->speed) == 0U) && (setup_data_length < w_length));
        if (status == xRETURN_OK)
        {
            xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_IN, setup_data_length);
        }
        return status;
    }

    if (setup_data_length > xUSBD_MAX_EP0_DATA_SIZE)
    {
        xUSBD_LOG(xRETURN_xWRN_xUSBD_BUFFER_MIGHT_OVERFLOW, "Control OUT data too large, stalling");
        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        return ep0_stall(device_ctx);
    }

    if (setup_data_length > 0U)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_OUT;
        xRETURN_t status =
            xUSBD_DCD_EP_Receive(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x00, device_ctx->control_data, setup_data_length);
        if (status == xRETURN_OK)
        {
            xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_OUT, setup_data_length);
        }
        return status;
    }

    device_ctx->request_phase = xUSBD_CTRL_PHASE_OUT_STATUS;
    xRETURN_t status = xUSBD_DCD_EP_Send(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x80U, device_ctx->control_data, 0x00U, false);
    if (status == xRETURN_OK)
    {
        xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_IN, 0U);
    }
    return status;
}

xRETURN_t xUSBD_EP0_Setup_Process(xUSBD_Device_Context_t *device_ctx)
{
    if (device_ctx == NULL)
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_NULL_POINTER, "NULL pointer exception");
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xRETURN_t status = xRETURN_OK;
    xUSBD_Response_t response = {NULL, 0U};

    xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_CONTROL_REQUEST,
                   ((uint32_t)device_ctx->request.bRequestType << 8U) | (uint32_t)device_ctx->request.bRequest);

    status = setup_request_dispatch(device_ctx, &response);

    if (status == xRETURN_OK)
    {
        status = setup_data_stage_start(device_ctx, response.data, response.length);
        if (status != xRETURN_OK)
        {
            device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        }
    }
    else
    {
        xRETURN_t stall_status;

        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        stall_status = ep0_stall(device_ctx);
        if (stall_status != xRETURN_OK)
        {
            status = stall_status;
        }
    }

    return status;
}

static xRETURN_t standard_get_string_descriptor_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_length = device_ctx->request.wLength;
    uint16_t w_value = device_ctx->request.wValue;
    uint8_t string_index = xU16_LOW_BYTE(w_value);
    uint8_t *string = NULL;

    xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_STRING, "Get string descriptor %d", string_index);

    if (string_index == xUSBD_LANG_ID_STRING_INDEX)
    {
        device_ctx->control_data[0] = 0x04;
        device_ctx->control_data[1] = USB_DESC_TYPE_STRING;
        device_ctx->control_data[2] = (uint8_t)(xUSBD_LANG_ID & 0xFF);
        device_ctx->control_data[3] = (uint8_t)((xUSBD_LANG_ID >> 8) & 0xFF);
        response->data = &device_ctx->control_data[0];
        response->length = MIN(w_length, 4);
        return xRETURN_OK;
    }

    if (string_index == xUSBD_VENDOR_STRING_INDEX)
    {
        string = device_ctx->vendor_string;
    }
    else if (string_index == xUSBD_PRODUCT_STRING_INDEX)
    {
        string = device_ctx->product_string;
    }
    else if (string_index == xUSBD_SERIAL_STRING_INDEX)
    {
        string = device_ctx->serial_string;
    }
    else if (string_index == xUSBD_MSOS_STRING_INDEX && device_ctx->mos2_length > 0)
    {
        static uint8_t mos_string[] = {'M', 'S', 'F', 'T', '1', '0', '0', xUSBD_WINUSB_VENDOR_CODE, '\0'};
        string = mos_string;
    }
    else
    {
        uint8_t *class_string = NULL;
        if (xUSBD_Class_Get_String_Process(device_ctx, string_index, &class_string) == xRETURN_OK && class_string != NULL)
        {
            string = class_string;
        }
        else
        {
            status = xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
        }
    }

    if (string != NULL)
    {
        status = string_to_string_descriptor(string, device_ctx->control_data);
        if (status != xRETURN_OK)
        {
            return status;
        }
        response->data = &device_ctx->control_data[0];
        response->length = MIN(w_length, device_ctx->control_data[0]);
    }

    return status;
}

static xRETURN_t standard_get_descriptor_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_length = device_ctx->request.wLength;
    uint16_t w_value = device_ctx->request.wValue;
    uint8_t descriptor_type = xU16_HIGH_BYTE(w_value);

    switch (descriptor_type)
    {
    case USB_DESC_TYPE_DEVICE:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_DEVICE, "Get device descriptor");
        response->data = device_ctx->device_descriptor;
        response->length = ((USB_Device_Descriptor_t *)device_ctx->device_descriptor)->bLength;
        response->length = MIN(w_length, response->length);
        break;
    case USB_DESC_TYPE_CONFIGURATION:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_CONFIG, "Get config descriptor");
        status = xUSBD_Build_Config_Descriptor(device_ctx, device_ctx->configuration_descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                               USB_DESC_TYPE_CONFIGURATION, device_ctx->speed);
        if (status == xRETURN_OK)
        {
            response->data = device_ctx->configuration_descriptor;
            response->length = xLE16_TO_CPU(((USB_Configuration_Descriptor_t *)device_ctx->configuration_descriptor)->wTotalLength);
            response->length = MIN(w_length, response->length);
        }
        break;

    case USB_DESC_TYPE_OTHER_SPEED:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_OTHER_SPEED, "Get other speed config descriptor");
        if (device_ctx->speed == USB_SPEED_HIGH || device_ctx->speed == USB_SPEED_FULL)
        {
            USB_Speed_t other_speed = (device_ctx->speed == USB_SPEED_HIGH) ? USB_SPEED_FULL : USB_SPEED_HIGH;
            status = xUSBD_Build_Config_Descriptor(device_ctx, device_ctx->configuration_descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                                   USB_DESC_TYPE_OTHER_SPEED, other_speed);
            if (status == xRETURN_OK)
            {
                response->data = device_ctx->configuration_descriptor;
                response->length = xLE16_TO_CPU(((USB_Configuration_Descriptor_t *)device_ctx->configuration_descriptor)->wTotalLength);
                response->length = MIN(w_length, response->length);
            }
        }
        else
        {
            status = xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
        }
        break;
    case USB_DESC_TYPE_STRING:
        status = standard_get_string_descriptor_process(device_ctx, response);
        break;
    case USB_DESC_TYPE_QUALIFIER:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_QUALIFIER, "Get qualifier descriptor");
        if (device_ctx->speed == USB_SPEED_SUPER)
        {
            status = xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
        }
        else
        {
            response->data = device_ctx->device_qualifier_descriptor;
            response->length = ((USB_Device_Qualifier_Descriptor_t *)device_ctx->device_qualifier_descriptor)->bLength;
            response->length = MIN(w_length, response->length);
        }
        break;

    case USB_DESC_TYPE_OTG:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_OTG, "Get otg descriptor");
        status = xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
        break;
    case USB_DESC_TYPE_BOS:
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_DESC_BOS, "Get bos descriptor");
        if (device_ctx->bos_length > 0)
        {
            response->data = device_ctx->bos_descriptor;
            response->length = MIN(w_length, device_ctx->bos_length);
        }
        else
        {
            status = xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
        }
        break;
    default:
        status = xUSBD_Class_In_Request_Process(device_ctx, response);
        break;
    }

    return status;
}

static xRETURN_t standard_get_status_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_index = device_ctx->request.wIndex;

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_STATUS_REMOTE_WAKE, "Get remote wake status");
        device_ctx->control_data[0] = 0x00;
        device_ctx->control_data[1] = 0x00U;
        if (device_ctx->is_remote_wake_enabled == true)
        {
            device_ctx->control_data[0] |= 0x02;
        }
        if (device_ctx->is_self_powered == true)
        {
            device_ctx->control_data[0] |= 0x01;
        }
        response->data = device_ctx->control_data;
        response->length = 2;
    }
    else if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_STATUS_INTF, "Get interface status");
        status = xUSBD_Class_In_Request_Process(device_ctx, response);
    }
    else if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_ENDPOINT)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_STATUS_EP, "Get endpoint status");
        device_ctx->control_data[0] = xUSBD_DCD_EP_Is_Stalled(device_ctx->dcd_ops, device_ctx->dcd_ctx, xU16_LOW_BYTE(w_index));
        device_ctx->control_data[1] = 0x00U;
        response->data = device_ctx->control_data;
        response->length = 2;
    }
    else
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_GET_STATUS_INVALID_RECIPIENT, "Get status, invalid recipient");
        status = xRETURN_xERR_xUSBD_REQ_GET_STATUS_INVALID_RECIPIENT;
    }

    return status;
}

static xRETURN_t standard_clear_feature_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;
    uint8_t feature_selector = xU16_LOW_BYTE(w_value);
    uint8_t endpoint_addr = xU16_LOW_BYTE(w_index);

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_ENDPOINT)
    {
        if (feature_selector == 0U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_CLEAR_EP_STALL, "Clear ep stall");
            status = xUSBD_DCD_EP_Clear_Stall(device_ctx->dcd_ops, device_ctx->dcd_ctx, endpoint_addr);
            if (status == xRETURN_OK)
            {
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_CLEAR_STALL, endpoint_addr);
            }
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_CLEAR_EP_INVALID_REQ, "Invalid endpoint clear feature");
            status = xRETURN_xERR_xUSBD_REQ_CLEAR_EP_INVALID_REQ;
        }
    }
    else if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        if (feature_selector == 1U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_CLEAR_REMOTE_WAKE, "Clear remote wake");
            status = xUSBD_DCD_Set_Remote_Wakeup(device_ctx->dcd_ops, device_ctx->dcd_ctx, false);
            if (status == xRETURN_OK)
            {
                device_ctx->is_remote_wake_enabled = false;
            }
        }
        else if (feature_selector == 0U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_CLEAR_TEST_MODE, "Clear test mode");
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_CLEAR_DEVICE_INVALID_REQ, "Invalid device clear feature");
            status = xRETURN_xERR_xUSBD_REQ_CLEAR_DEVICE_INVALID_REQ;
        }
    }
    else
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_CLEAR_INVALID_RECIPIENT, "Clear feature, invalid recipient");
        status = xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
    }

    return status;
}

static xRETURN_t standard_set_feature_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;
    uint8_t feature_selector = xU16_LOW_BYTE(w_value);
    uint8_t endpoint_addr = xU16_LOW_BYTE(w_index);

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_ENDPOINT)
    {
        if (feature_selector == 0U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_EP_STALL, "Set ep stall");
            status = xUSBD_DCD_EP_Stall(device_ctx->dcd_ops, device_ctx->dcd_ctx, endpoint_addr);
            if (status == xRETURN_OK)
            {
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_STALL, endpoint_addr);
            }
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_FEATURE_EP_INVALID_REQ, "Invalid endpoint set feature");
            status = xRETURN_xERR_xUSBD_REQ_SET_FEATURE_EP_INVALID_REQ;
        }
    }
    else if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        if (feature_selector == 1U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_REMOTE_WAKE, "Set remote wake");
            status = xUSBD_DCD_Set_Remote_Wakeup(device_ctx->dcd_ops, device_ctx->dcd_ctx, true);
            if (status == xRETURN_OK)
            {
                device_ctx->is_remote_wake_enabled = true;
            }
        }
        else if (feature_selector == 0U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_FEATURE_TEST_MODE, "Set test mode");
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_FEATURE_DEVICE_INVALID_REQ, "Invalid device set feature");
            status = xRETURN_xERR_xUSBD_REQ_SET_FEATURE_DEVICE_INVALID_REQ;
        }
    }
    else
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_FEATURE_INVALID_RECIPIENT, "Set feature, invalid recipient");
        status = xRETURN_xERR_xUSBD_REQ_SET_FEATURE_INVALID_RECIPIENT;
    }

    return status;
}

static xRETURN_t standard_set_address_process(xUSBD_Device_Context_t *device_ctx)
{
    uint16_t w_value = device_ctx->request.wValue;

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_ADDRESS, "Set address");
        device_ctx->address_value = xU16_LOW_BYTE(w_value);
        return xRETURN_OK;
    }

    xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_ADDRESS_INVALID_REQ, "Set address, invalid recipient");
    return xRETURN_xERR_xUSBD_REQ_SET_ADDRESS_INVALID_REQ;
}

static xRETURN_t standard_get_configuration_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    if (device_ctx->is_configured)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_CONFIG_CONFIGURED, "Get config, configured");
        device_ctx->control_data[0] = device_ctx->configuration_value;
    }
    else if (device_ctx->is_addressed)
    {
        xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_GET_CONFIG_ADDRESSED, "Get config, addressed");
        device_ctx->control_data[0] = 0;
    }
    else
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_GET_CONFIG_NOT_ADDRESSED, "Get config, not addressed");
        return xRETURN_xERR_xUSBD_REQ_GET_CONFIG_NOT_ADDRESSED;
    }

    response->data = device_ctx->control_data;
    response->length = 1;
    return xRETURN_OK;
}

static xRETURN_t standard_set_configuration_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xRETURN_OK;
    uint16_t w_value = device_ctx->request.wValue;
    uint8_t configuration = xU16_LOW_BYTE(w_value);

    if (device_ctx->is_addressed)
    {
        if (configuration == 0U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_CONFIG_ZERO, "Set config as 0");
            device_ctx->is_configured = false;
            device_ctx->configuration_value = 0U;
        }
        else if (configuration == 1U)
        {
            xUSBD_LOG(xRETURN_xMSG_xUSBD_REQ_SET_CONFIG_ONE, "Set config as 1");
            device_ctx->is_configured = true;
            device_ctx->configuration_value = 1U;
        }
        else
        {
            xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_SUPPORTED, "Supports only one configuration");
            status = xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_SUPPORTED;
        }
    }
    else
    {
        xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_ADDRESSED, "Set config, not addressed");
        status = xRETURN_xERR_xUSBD_REQ_SET_CONFIG_NOT_ADDRESSED;
    }

    return status;
}

static xRETURN_t standard_get_interface_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    uint16_t w_index = device_ctx->request.wIndex;
    uint8_t interface_idx = xU16_LOW_BYTE(w_index);

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE)
    {
        xRETURN_t status = xUSBD_Class_In_Request_Process(device_ctx, response);
        if (status != xRETURN_OK && interface_idx < device_ctx->next_interface)
        {
            device_ctx->control_data[0] = 0x00U;
            response->data = device_ctx->control_data;
            response->length = 1;
            return xRETURN_OK;
        }
        return status;
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t standard_set_interface_process(xUSBD_Device_Context_t *device_ctx)
{
    uint16_t w_length = device_ctx->request.wLength;
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;
    uint8_t interface_idx = xU16_LOW_BYTE(w_index);

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_INTERFACE)
    {
        xRETURN_t status = xUSBD_Class_Out_Request_Process(device_ctx, device_ctx->control_data, w_length);
        if (status != xRETURN_OK && w_value == 0U && interface_idx < device_ctx->next_interface)
        {
            return xRETURN_OK;
        }
        return status;
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t standard_set_sel_process(xUSBD_Device_Context_t *device_ctx, uint32_t *setup_data_length)
{
    uint16_t w_length = device_ctx->request.wLength;
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;

    if (device_ctx->speed == USB_SPEED_SUPER && (device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE &&
        (device_ctx->request.bRequestType & USB_REQ_TYPE_IN) == USB_REQ_TYPE_OUT && w_value == 0U && w_index == 0U &&
        w_length == sizeof(device_ctx->usb3_system_exit_latency))
    {
        *setup_data_length = sizeof(device_ctx->usb3_system_exit_latency);
        return xRETURN_OK;
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t standard_set_isoch_delay_process(xUSBD_Device_Context_t *device_ctx)
{
    uint16_t w_length = device_ctx->request.wLength;
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;

    if (device_ctx->speed == USB_SPEED_SUPER && (device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE &&
        (device_ctx->request.bRequestType & USB_REQ_TYPE_IN) == USB_REQ_TYPE_OUT && w_index == 0U && w_length == 0U)
    {
        device_ctx->usb3_isoch_delay = w_value;
        return xRETURN_OK;
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t standard_request_process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    xASSERT(device_ctx != NULL && response != NULL, "null argument to standard_request_process");

    xRETURN_t status = xRETURN_OK;

    switch (device_ctx->request.bRequest)
    {
    case USB_REQ_GET_STATUS:
        status = standard_get_status_process(device_ctx, response);
        break;
    case USB_REQ_CLEAR_FEATURE:
        status = standard_clear_feature_process(device_ctx);
        break;
    case USB_REQ_SET_FEATURE:
        status = standard_set_feature_process(device_ctx);
        break;
    case USB_REQ_SET_ADDRESS:
        status = standard_set_address_process(device_ctx);
        break;
    case USB_REQ_GET_DESCRIPTOR:
        status = standard_get_descriptor_process(device_ctx, response);
        break;
    case USB_REQ_SET_DESCRIPTOR:
        xUSBD_LOG(xRETURN_xERR_xUSBD_REQ_SET_DESC_NOT_SUPPORTED, "Set descriptor, not supported");
        status = xRETURN_xERR_xUSBD_REQ_SET_DESC_NOT_SUPPORTED;
        break;
    case USB_REQ_GET_CONFIGURATION:
        status = standard_get_configuration_process(device_ctx, response);
        break;
    case USB_REQ_SET_CONFIGURATION:
        status = standard_set_configuration_process(device_ctx);
        break;
    case USB_REQ_GET_INTERFACE:
        status = standard_get_interface_process(device_ctx, response);
        break;
    case USB_REQ_SET_INTERFACE:
        status = standard_set_interface_process(device_ctx);
        break;
    case USB_REQ_SYNCH_FRAME:
        status = xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
        break;
    case USB_REQ_SET_SEL:
        status = standard_set_sel_process(device_ctx, &response->length);
        break;
    case USB_REQ_SET_ISOCH_DELAY:
        status = standard_set_isoch_delay_process(device_ctx);
        break;
    default:
        status = xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
        break;
    }

    return status;
}

static xRETURN_t standard_out_data_process(xUSBD_Device_Context_t *device_ctx, uint8_t *data, uint32_t length)
{
    if (device_ctx == NULL || data == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (device_ctx->request.bRequest == USB_REQ_SET_SEL && device_ctx->speed == USB_SPEED_SUPER &&
        length == sizeof(device_ctx->usb3_system_exit_latency))
    {
        memcpy(device_ctx->usb3_system_exit_latency, data, sizeof(device_ctx->usb3_system_exit_latency));
        return xRETURN_OK;
    }

    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xRETURN_t status_set_feature_complete_process(xUSBD_Device_Context_t *device_ctx)
{
    uint16_t w_value = device_ctx->request.wValue;
    uint16_t w_index = device_ctx->request.wIndex;

    if (xU16_LOW_BYTE(w_value) == 0U)
    {
        return xUSBD_DCD_Set_Test_Mode(device_ctx->dcd_ops, device_ctx->dcd_ctx, xU16_HIGH_BYTE(w_index));
    }

    return xRETURN_OK;
}

static xRETURN_t status_set_address_complete_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xUSBD_DCD_Set_Address(device_ctx->dcd_ops, device_ctx->dcd_ctx, device_ctx->address_value);
    if (status == xRETURN_OK)
    {
        device_ctx->is_addressed = true;
        xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_SET_ADDRESS, device_ctx->address_value);
    }

    return status;
}

static xRETURN_t status_set_configuration_complete_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xRETURN_OK;

    if (device_ctx->is_configured)
    {
        device_ctx->lifecycle_state = xUSBD_LIFECYCLE_CONFIGURED;
    }
    else
    {
        device_ctx->lifecycle_state = xUSBD_LIFECYCLE_STARTED;
    }

    xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_SET_CONFIGURATION, device_ctx->configuration_value);

    return status;
}

static xRETURN_t status_phase_complete_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = xRETURN_OK;

    if (device_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        switch (device_ctx->request.bRequest)
        {
        case USB_REQ_SET_FEATURE:
            status = status_set_feature_complete_process(device_ctx);
            break;
        case USB_REQ_SET_ADDRESS:
            status = status_set_address_complete_process(device_ctx);
            break;
        case USB_REQ_SET_CONFIGURATION:
            status = status_set_configuration_complete_process(device_ctx);
            break;
        default:
            break;
        }
    }

    return status;
}

static xRETURN_t control_transfer_complete_process(xUSBD_Device_Context_t *device_ctx)
{
    xRETURN_t status = status_phase_complete_process(device_ctx);
    if (status == xRETURN_OK)
    {
        status = xUSBD_Class_Control_Transfer_Complete_Process(device_ctx, &device_ctx->request);
    }

    return status;
}

static xRETURN_t ep0_out_data_received_process(xUSBD_Device_Context_t *device_ctx, uint32_t length)
{
    xRETURN_t status = xRETURN_OK;

    if ((device_ctx->request.bRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD)
    {
        status = standard_out_data_process(device_ctx, device_ctx->control_data, length);
    }
    else
    {
        status = xUSBD_Class_Out_Request_Process(device_ctx, device_ctx->control_data, length);
    }

    if (status == xRETURN_OK)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_OUT_STATUS;
        status = xUSBD_DCD_EP_Send(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x80U, device_ctx->control_data, 0x00U, false);
        if (status != xRETURN_OK)
        {
            device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        }
        else
        {
            xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_EP_IN, 0U);
        }
    }
    else
    {
        xRETURN_t stall_status;

        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        stall_status = ep0_stall(device_ctx);
        if (stall_status != xRETURN_OK)
        {
            status = stall_status;
        }
    }

    return status;
}

xRETURN_t xUSBD_EP0_Data_Received_Process(xUSBD_Device_Context_t *device_ctx, uint32_t length)
{
    if (device_ctx->request_phase == xUSBD_CTRL_PHASE_OUT)
    {
        return ep0_out_data_received_process(device_ctx, length);
    }

    if (device_ctx->request_phase == xUSBD_CTRL_PHASE_IN)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        return control_transfer_complete_process(device_ctx);
    }

    return xRETURN_OK;
}

xRETURN_t xUSBD_EP0_Data_Sent_Process(xUSBD_Device_Context_t *device_ctx)
{
    if (device_ctx->request_phase == xUSBD_CTRL_PHASE_OUT_STATUS)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        return control_transfer_complete_process(device_ctx);
    }

    return xRETURN_OK;
}

xRETURN_t xUSBD_EP0_Configure(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed)
{
    uint16_t ep0_size = ep0_mps(speed);
    xRETURN_t status;

    status = xUSBD_DCD_EP_Init(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x00, USB_ENDP_TYPE_CTRL, ep0_size);
    if (status == xRETURN_OK)
    {
        status = xUSBD_DCD_EP_Init(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x80U, USB_ENDP_TYPE_CTRL, ep0_size);
    }
    if (status == xRETURN_OK)
    {
        device_ctx->request_phase = xUSBD_CTRL_PHASE_IDLE;
        status = xUSBD_DCD_EP_Receive(device_ctx->dcd_ops, device_ctx->dcd_ctx, 0x00, device_ctx->control_data, 8);
    }

    return status;
}
// EOF /////////////////////////////////////////////////////////////////////////////
