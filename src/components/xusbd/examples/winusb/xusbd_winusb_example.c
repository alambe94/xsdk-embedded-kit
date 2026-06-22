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

// @file xusbd_winusb_example.c
// @brief Application-level hooks and configuration for WinUSB.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_win.h"
#include "xusbd_winusb_example.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static uint32_t win_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static uint32_t win_app_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static uint32_t win_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
//

static uint8_t WIN_RX_Buffer[512];

static xUSBD_WIN_App_Context_t *win_app_context_get(xUSBD_Class_Context_t *class_ctx)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return NULL;
    }

    return (xUSBD_WIN_App_Context_t *)app_context;
}

static uint32_t win_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_WIN_App_Context_t *app_info = win_app_context_get(class_ctx);
    if (app_info == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    switch (event)
    {
    case USB_DCD_CONNECT_RECEIVED:
        app_info->reset_complete = true;
        break;

    case USB_DCD_SOF_RECEIVED: /* fall through */
    default:
        break;
    }
    return xRETURN_OK;
}

static uint32_t win_app_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    xUSBD_WIN_App_Context_t *app_info = win_app_context_get(class_ctx);
    if (app_info == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    app_info->rx_complete = true;
    app_info->rx_length = length;

    return xRETURN_OK;
}

static uint32_t win_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    xUSBD_WIN_App_Context_t *app_info = win_app_context_get(class_ctx);
    if (app_info == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    app_info->tx_complete = true;

    return xRETURN_OK;
}

void xUSBD_WIN_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_WIN_App_Context_t *app_context)
{
    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_WIN_Callbacks_t callbacks = {
        .on_bus_event = win_app_bus_event,
        .on_data_received = win_app_data_received,
        .on_transmit_complete = win_app_transmit_complete,
    };
    (void)xUSBD_WIN_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_WIN_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_WIN_App_Context_t *app_info = win_app_context_get(class_ctx);
    if (app_info == NULL)
    {
        return;
    }

    switch (app_info->state)
    {
    case xUSBD_WIN_APP_STATE_IDLE:
        if (app_info->reset_complete)
        {
            app_info->reset_complete = false;
            xUSBD_WIN_Prepare_To_Receive(class_ctx, WIN_RX_Buffer, sizeof(WIN_RX_Buffer));
            app_info->state = xUSBD_WIN_APP_STATE_ECHO;
        }
        break;

    case xUSBD_WIN_APP_STATE_ECHO:
        if (app_info->rx_complete)
        {
            app_info->rx_complete = false;
            xUSBD_WIN_Transmit(class_ctx, WIN_RX_Buffer, app_info->rx_length, false);
        }

        if (app_info->tx_complete)
        {
            app_info->tx_complete = false;
            xUSBD_WIN_Prepare_To_Receive(class_ctx, WIN_RX_Buffer, sizeof(WIN_RX_Buffer));
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
