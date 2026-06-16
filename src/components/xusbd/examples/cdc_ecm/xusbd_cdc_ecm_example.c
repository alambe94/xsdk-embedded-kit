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

// @file xusbd_cdc_ecm_example.c
// @brief Application-level hooks and configuration for USB CDC ECM.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_cdc.h"
#include "xusbd_cdc_ecm_example.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t cdc_ecm_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t cdc_ecm_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t cdc_ecm_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
static xRETURN_t cdc_ecm_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t cdc_ecm_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xRETURN_t cdc_ecm_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_ECM_Context_t *ecm_info = (xUSBD_CDC_ECM_Context_t *)app_context;

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        ecm_info->reset_complete = true;
        break;

    case USB_DCD_SOF_RECEIVED: /* fall through */
    default:
        break;
    }
    return xRETURN_OK;
}

static xRETURN_t cdc_ecm_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    (void)class_ctx;
    (void)response;
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_ecm_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    (void)control_data;
    (void)length;
    uint8_t b_request = class_ctx->device_ctx->request.bRequest;
    if (b_request == USB_CDC_ECM_SET_ETHERNET_PACKET_FILTER || b_request == USB_CDC_ECM_SET_ETHERNET_MULTICAST_FILTERS)
    {
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_ecm_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_ECM_Context_t *ecm_info = (xUSBD_CDC_ECM_Context_t *)app_context;

    ecm_info->rx_complete = true;
    ecm_info->rx_length = length;
    return xRETURN_OK;
}

static xRETURN_t cdc_ecm_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_ECM_Context_t *ecm_info = (xUSBD_CDC_ECM_Context_t *)app_context;

    ecm_info->tx_complete = true;
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_CDC_ECM_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_ECM_Context_t *app_context)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    app_context->reset_complete = false;
    app_context->tx_complete = false;
    app_context->rx_complete = false;
    app_context->rx_length = 0U;
    app_context->state = xUSBD_CDC_ECM_STATE_INIT;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);
    (void)xUSBD_Class_Set_Interface_String(class_ctx, "001122334455");

    ctx->subclass = 0x06U;
    ctx->protocol = 0x00U;
    ctx->cmd_ep_interval = 0x08U;
    ctx->cmd_ep_mps = 64U;
    ctx->has_notification_ep = true;

    static xUSBD_CDC_Callbacks_t callbacks = {
        .on_bus_event = cdc_ecm_bus_event,
        .on_control_in = cdc_ecm_control_in,
        .on_control_out = cdc_ecm_control_out,
        .on_data_received = cdc_ecm_data_received,
        .on_transmit_complete = cdc_ecm_transmit_complete,
    };
    (void)xUSBD_CDC_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_CDC_ECM_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return;
    }

    xUSBD_CDC_ECM_Context_t *ecm_info = (xUSBD_CDC_ECM_Context_t *)app_context;

    switch (ecm_info->state)
    {
    case xUSBD_CDC_ECM_STATE_INIT:
        if (ecm_info->reset_complete == true)
        {
            static uint8_t s_connected_notif[8];
            ecm_info->reset_complete = false;
            ecm_info->state = xUSBD_CDC_ECM_STATE_READY;
            s_connected_notif[0] = 0xA1U;
            s_connected_notif[1] = USB_CDC_NOTIF_NETWORK_CONNECTION;
            s_connected_notif[2] = 0x01U;
            s_connected_notif[3] = 0x00U;
            s_connected_notif[4] = ctx->cmd_interface;
            s_connected_notif[5] = 0x00U;
            s_connected_notif[6] = 0x00U;
            s_connected_notif[7] = 0x00U;
            (void)xUSBD_CDC_Send_Notification(class_ctx, s_connected_notif, sizeof(s_connected_notif));
            (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, ecm_info->rx_buffer, sizeof(ecm_info->rx_buffer));
        }
        break;

    case xUSBD_CDC_ECM_STATE_READY:
        if (ecm_info->rx_complete == true)
        {
            ecm_info->rx_complete = false;
            ecm_info->state = xUSBD_CDC_ECM_STATE_TRANSMITTING;
            (void)xUSBD_CDC_Transmit(class_ctx, ecm_info->rx_buffer, ecm_info->rx_length);
        }
        break;

    case xUSBD_CDC_ECM_STATE_TRANSMITTING:
        if (ecm_info->tx_complete == true)
        {
            ecm_info->tx_complete = false;
            ecm_info->state = xUSBD_CDC_ECM_STATE_READY;
            (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, ecm_info->rx_buffer, sizeof(ecm_info->rx_buffer));
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
