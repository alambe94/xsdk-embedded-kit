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

// @file xusbd_cdc_acm_example.c
// @brief Application-level hooks and configuration for USB CDC ACM.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_cdc.h"
#include "xusbd_cdc_acm_example.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////
static uint8_t CDC_RX_Data[128];
static uint8_t CDC_TX_Data[512] = "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n"
                                  "Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!Hello!!!!!\r\n";

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t cdc_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t cdc_app_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t cdc_app_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
static xRETURN_t cdc_app_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t cdc_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xRETURN_t cdc_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        cdc_info->reset_complete = true;
        break;

    case USB_DCD_SOF_RECEIVED: /* fall through */
    default:
        break;
    }
    return xRETURN_OK;
}

static xRETURN_t cdc_app_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    if (class_ctx->device_ctx->request.bRequest == USB_CDC_GET_LINE_CODING)
    {
        void *app_context = NULL;
        if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
        {
            return xRETURN_xERR_xUSBD_NULL_POINTER;
        }
        xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;

        uint8_t *buffer = NULL;
        uint32_t buffer_length = 0U;
        xRETURN_t status = xUSBD_Class_Get_Control_Buffer(class_ctx, &buffer, &buffer_length);
        if (status != xRETURN_OK)
        {
            return status;
        }
        if (buffer_length < sizeof(USB_CDC_Line_Code_t))
        {
            return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
        }
        (void)memcpy(buffer, &cdc_info->line_coding, sizeof(USB_CDC_Line_Code_t));
        response->data = buffer;
        response->length = sizeof(USB_CDC_Line_Code_t);
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_app_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    if (class_ctx->device_ctx->request.bRequest == USB_CDC_SET_LINE_CODING)
    {
        if (length >= sizeof(USB_CDC_Line_Code_t))
        {
            void *app_context = NULL;
            if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
            {
                return xRETURN_xERR_xUSBD_NULL_POINTER;
            }
            xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;
            (void)memcpy(&cdc_info->line_coding, control_data, sizeof(USB_CDC_Line_Code_t));
            return xRETURN_OK;
        }
    }
    else if (class_ctx->device_ctx->request.bRequest == USB_CDC_SET_CONTROL_LINE_STATE)
    {
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_app_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;

    cdc_info->rx_complete = true;
    cdc_info->rx_length = length;
    return xRETURN_OK;
}

static xRETURN_t cdc_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;

    cdc_info->tx_complete = true;
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_CDC_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_App_Context_t *app_context)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    app_context->reset_complete = false;
    app_context->line_coding_received = false;
    app_context->tx_complete = false;
    app_context->rx_complete = false;
    app_context->rx_length = 0U;
    app_context->state = xUSBD_CDC_APP_STATE_INIT;
    app_context->line_coding.baud_rate = 115200U;
    app_context->line_coding.data_bits = 8U;
    app_context->line_coding.parity = 0U;
    app_context->line_coding.stop_bits = 1U;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    ctx->subclass = 0x02U;
    ctx->protocol = 0x01U;
    ctx->cmd_ep_interval = 0x08U;
    ctx->cmd_ep_mps = 64U;
    ctx->has_notification_ep = true;

    static xUSBD_CDC_Callbacks_t callbacks = {
        .on_bus_event = cdc_app_bus_event,
        .on_control_in = cdc_app_control_in,
        .on_control_out = cdc_app_control_out,
        .on_data_received = cdc_app_data_received,
        .on_transmit_complete = cdc_app_transmit_complete,
    };
    (void)xUSBD_CDC_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_CDC_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return;
    }

    xUSBD_CDC_App_Context_t *cdc_info = (xUSBD_CDC_App_Context_t *)app_context;

    switch (cdc_info->state)
    {
    case xUSBD_CDC_APP_STATE_INIT:
        if (cdc_info->reset_complete == true)
        {
            cdc_info->reset_complete = false;
            cdc_info->state = xUSBD_CDC_APP_STATE_SEND_FIRST_MESSAGE;
        }
        break;

    case xUSBD_CDC_APP_STATE_WAIT_LINE_CODING:
        if (cdc_info->line_coding_received == true)
        {
            cdc_info->line_coding_received = false;
            cdc_info->state = xUSBD_CDC_APP_STATE_SEND_FIRST_MESSAGE;
        }
        break;

    case xUSBD_CDC_APP_STATE_SEND_FIRST_MESSAGE:
        (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, CDC_RX_Data, sizeof(CDC_RX_Data));
        (void)xUSBD_CDC_Transmit(class_ctx, CDC_TX_Data, (uint32_t)strlen((char *)CDC_TX_Data));

        if (cdc_info->tx_complete == true)
        {
            cdc_info->tx_complete = false;
            cdc_info->state = xUSBD_CDC_APP_STATE_ECHO;
        }
        break;

    case xUSBD_CDC_APP_STATE_ECHO:
        if (cdc_info->rx_complete == true)
        {
            cdc_info->rx_complete = false;
            (void)xUSBD_CDC_Transmit(class_ctx, CDC_RX_Data, cdc_info->rx_length);
        }

        if (cdc_info->tx_complete == true)
        {
            cdc_info->tx_complete = false;
            (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, CDC_RX_Data, sizeof(CDC_RX_Data));
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
