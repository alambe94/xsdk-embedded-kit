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

// @file xusbd_cdc_ncm_example.c
// @brief Application-level hooks and configuration for USB CDC NCM.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_cdc.h"
#include "xusbd_cdc_ncm_example.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////
#pragma pack(push, 1)

typedef struct
{
    uint32_t dwSignature; // "NTH1" = 0x3148544EU (little-endian)
    uint16_t wHeaderLength;
    uint16_t wSequence;
    uint16_t wBlockLength;
    uint16_t wNdpIndex;
} USB_CDC_NCM_Nth16_t;

typedef struct
{
    uint16_t wDatagramIndex;
    uint16_t wDatagramLength;
} USB_CDC_NCM_DatagramPointer_t;

typedef struct
{
    uint32_t dwSignature; // "NDP1" = 0x3144504EU (little-endian, no FCS)
    uint16_t wLength;
    uint16_t wNextNdpIndex;
    USB_CDC_NCM_DatagramPointer_t datagrams[2];
} USB_CDC_NCM_Ndp16_t;

#pragma pack(pop)

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t cdc_ncm_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t cdc_ncm_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t cdc_ncm_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
static xRETURN_t cdc_ncm_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t cdc_ncm_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xRETURN_t cdc_ncm_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_NCM_Context_t *ncm_info = (xUSBD_CDC_NCM_Context_t *)app_context;

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        ncm_info->reset_complete = true;
        break;

    case USB_DCD_SOF_RECEIVED: /* fall through */
    default:
        break;
    }
    return xRETURN_OK;
}

static xRETURN_t cdc_ncm_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    uint8_t b_request = class_ctx->device_ctx->request.bRequest;
    if (b_request == USB_CDC_NCM_GET_NTB_PARAMETERS)
    {
        uint8_t *buffer = NULL;
        uint32_t buffer_length = 0U;
        xRETURN_t status = xUSBD_Class_Get_Control_Buffer(class_ctx, &buffer, &buffer_length);
        USB_CDC_NCM_NTB_Parameters_t *params = (USB_CDC_NCM_NTB_Parameters_t *)buffer;
        if (status != xRETURN_OK)
        {
            return status;
        }
        if (buffer_length < sizeof(USB_CDC_NCM_NTB_Parameters_t))
        {
            return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
        }
        params->wLength = xCPU_TO_LE16((uint16_t)sizeof(USB_CDC_NCM_NTB_Parameters_t));
        params->bmNtbFormatsSupported = xCPU_TO_LE16(0x0001U); // NTB-16 only
        params->dwNtbInMaxSize = xCPU_TO_LE32(2048U);
        params->wNdpInDivisor = xCPU_TO_LE16(4U);
        params->wNdpInPayloadRemainder = xCPU_TO_LE16(0U);
        params->wNdpInAlignment = xCPU_TO_LE16(4U);
        params->wReserved = 0U;
        params->dwNtbOutMaxSize = xCPU_TO_LE32(2048U);
        params->wNdpOutDivisor = xCPU_TO_LE16(4U);
        params->wNdpOutPayloadRemainder = xCPU_TO_LE16(0U);
        params->wNdpOutAlignment = xCPU_TO_LE16(4U);
        params->wNtbOutMaxDatagrams = 0U; // no limit
        response->data = buffer;
        response->length = sizeof(USB_CDC_NCM_NTB_Parameters_t);
        return xRETURN_OK;
    }
    else if (b_request == USB_CDC_NCM_GET_NTB_FORMAT)
    {
        uint8_t *buffer = NULL;
        uint32_t buffer_length = 0U;
        xRETURN_t status = xUSBD_Class_Get_Control_Buffer(class_ctx, &buffer, &buffer_length);
        if (status != xRETURN_OK)
        {
            return status;
        }
        buffer[0] = 0U; // NTB-16
        response->data = buffer;
        response->length = 1U;
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_ncm_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    (void)control_data;
    (void)length;
    uint8_t b_request = class_ctx->device_ctx->request.bRequest;
    if (b_request == USB_CDC_NCM_SET_NTB_FORMAT || b_request == USB_CDC_NCM_SET_NTB_INPUT_SIZE ||
        b_request == USB_CDC_NCM_SET_MAX_DATAGRAM_SIZE)
    {
        return xRETURN_OK;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t cdc_ncm_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_NCM_Context_t *ncm_info = (xUSBD_CDC_NCM_Context_t *)app_context;

    ncm_info->rx_complete = true;
    ncm_info->rx_length = length;
    return xRETURN_OK;
}

static xRETURN_t cdc_ncm_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_CDC_NCM_Context_t *ncm_info = (xUSBD_CDC_NCM_Context_t *)app_context;

    ncm_info->tx_complete = true;
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_CDC_NCM_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_CDC_NCM_Context_t *app_context)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;

    app_context->reset_complete = false;
    app_context->tx_complete = false;
    app_context->rx_complete = false;
    app_context->rx_length = 0U;
    app_context->state = xUSBD_CDC_NCM_STATE_INIT;
    app_context->tx_length = 0U;
    app_context->sequence = 0U;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);
    (void)xUSBD_Class_Set_Interface_String(class_ctx, "001122334455");

    ctx->subclass = 0x0DU;
    ctx->protocol = 0x00U;
    ctx->cmd_ep_interval = 0x08U;
    ctx->cmd_ep_mps = 64U;
    ctx->has_notification_ep = true;

    static xUSBD_CDC_Callbacks_t callbacks = {
        .on_bus_event = cdc_ncm_bus_event,
        .on_control_in = cdc_ncm_control_in,
        .on_control_out = cdc_ncm_control_out,
        .on_data_received = cdc_ncm_data_received,
        .on_transmit_complete = cdc_ncm_transmit_complete,
    };
    (void)xUSBD_CDC_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_CDC_NCM_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_CDC_Context_t *ctx = (xUSBD_CDC_Context_t *)class_ctx;
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return;
    }

    xUSBD_CDC_NCM_Context_t *ncm_info = (xUSBD_CDC_NCM_Context_t *)app_context;

    switch (ncm_info->state)
    {
    case xUSBD_CDC_NCM_STATE_INIT:
        if (ncm_info->reset_complete == true)
        {
            static uint8_t s_connected_notif[8];
            ncm_info->reset_complete = false;
            ncm_info->state = xUSBD_CDC_NCM_STATE_READY;
            s_connected_notif[0] = 0xA1U;
            s_connected_notif[1] = USB_CDC_NOTIF_NETWORK_CONNECTION;
            s_connected_notif[2] = 0x01U;
            s_connected_notif[3] = 0x00U;
            s_connected_notif[4] = ctx->cmd_interface;
            s_connected_notif[5] = 0x00U;
            s_connected_notif[6] = 0x00U;
            s_connected_notif[7] = 0x00U;
            (void)xUSBD_CDC_Send_Notification(class_ctx, s_connected_notif, sizeof(s_connected_notif));
            (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, ncm_info->rx_data, sizeof(ncm_info->rx_data));
        }
        break;

    case xUSBD_CDC_NCM_STATE_READY:
        if (ncm_info->rx_complete == true)
        {
            USB_CDC_NCM_Nth16_t *nth_rx = (USB_CDC_NCM_Nth16_t *)ncm_info->rx_data;
            USB_CDC_NCM_Ndp16_t *ndp_rx = NULL;

            ncm_info->rx_complete = false;
            ncm_info->tx_length = 0U;

            if (ncm_info->rx_length >= sizeof(USB_CDC_NCM_Nth16_t) && nth_rx->dwSignature == 0x3148544EU)
            {
                uint16_t ndp_offset = nth_rx->wNdpIndex;
                if ((uint32_t)ndp_offset + sizeof(USB_CDC_NCM_Ndp16_t) <= ncm_info->rx_length)
                {
                    ndp_rx = (USB_CDC_NCM_Ndp16_t *)(ncm_info->rx_data + ndp_offset);
                    uint16_t dg_idx = ndp_rx->datagrams[0].wDatagramIndex;
                    uint16_t dg_len = ndp_rx->datagrams[0].wDatagramLength;
                    if (dg_idx > 0U && dg_len > 0U && (uint32_t)(dg_idx + dg_len) <= ncm_info->rx_length)
                    {
                        USB_CDC_NCM_Nth16_t *nth_tx = (USB_CDC_NCM_Nth16_t *)ncm_info->tx_data;
                        USB_CDC_NCM_Ndp16_t *ndp_tx = (USB_CDC_NCM_Ndp16_t *)(ncm_info->tx_data + 12U);
                        nth_tx->dwSignature = 0x3148544EU;
                        nth_tx->wHeaderLength = 12U;
                        nth_tx->wSequence = ncm_info->sequence++;
                        nth_tx->wNdpIndex = 12U;
                        ndp_tx->dwSignature = 0x3144504EU;
                        ndp_tx->wLength = 16U;
                        ndp_tx->wNextNdpIndex = 0U;
                        ndp_tx->datagrams[0].wDatagramIndex = 28U;
                        ndp_tx->datagrams[0].wDatagramLength = dg_len;
                        ndp_tx->datagrams[1].wDatagramIndex = 0U;
                        ndp_tx->datagrams[1].wDatagramLength = 0U;
                        (void)memcpy(ncm_info->tx_data + 28U, ncm_info->rx_data + dg_idx, dg_len);
                        nth_tx->wBlockLength = (uint16_t)(28U + dg_len);
                        ncm_info->tx_length = 28U + dg_len;
                    }
                }
            }

            if (ncm_info->tx_length > 0U)
            {
                ncm_info->state = xUSBD_CDC_NCM_STATE_TRANSMITTING;
                (void)xUSBD_CDC_Transmit(class_ctx, ncm_info->tx_data, ncm_info->tx_length);
            }
            else
            {
                (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, ncm_info->rx_data, sizeof(ncm_info->rx_data));
            }
        }
        break;

    case xUSBD_CDC_NCM_STATE_TRANSMITTING:
        if (ncm_info->tx_complete == true)
        {
            ncm_info->tx_complete = false;
            ncm_info->state = xUSBD_CDC_NCM_STATE_READY;
            (void)xUSBD_CDC_Prepare_To_Receive(class_ctx, ncm_info->rx_data, sizeof(ncm_info->rx_data));
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
