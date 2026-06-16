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

// @file xusbd_hid_mouse_example.c
// @brief Application-level hooks and configuration for USB HID Mouse.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include "string.h"

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_hid.h"
#include "xusbd_hid_mouse_example.h"
#include "xusbd_return.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
const uint8_t g_xUSBD_Mouse_Report_Descriptor[] = {0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19,
                                                   0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02,
                                                   0x75, 0x05, 0x95, 0x01, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09,
                                                   0x38, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x03, 0x81, 0x06, 0xC0, 0xC0};

const uint16_t g_xUSBD_Mouse_Report_Descriptor_Len = (uint16_t)sizeof(g_xUSBD_Mouse_Report_Descriptor);

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xUSBD_HID_Mouse_App_Context_t *hid_mouse_app_context_get(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t hid_mouse_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t hid_mouse_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static void hid_mouse_send_position(xUSBD_Class_Context_t *mouse_ctx, int8_t x, int8_t y);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xUSBD_HID_Mouse_App_Context_t *hid_mouse_app_context_get(xUSBD_Class_Context_t *class_ctx)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return NULL;
    }
    return (xUSBD_HID_Mouse_App_Context_t *)app_context;
}

static xRETURN_t hid_mouse_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_HID_Mouse_App_Context_t *app_context = hid_mouse_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (event == USB_DCD_CONNECT_RECEIVED)
    {
        app_context->reset_complete = true;
    }

    return xRETURN_OK;
}

static xRETURN_t hid_mouse_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    xUSBD_HID_Mouse_App_Context_t *app_context = hid_mouse_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    app_context->mouse_tx_complete = true;
    return xRETURN_OK;
}

static void hid_mouse_send_position(xUSBD_Class_Context_t *mouse_ctx, int8_t x, int8_t y)
{
    int8_t mouse_report[4] = {0};
    mouse_report[1] = x;
    mouse_report[2] = y;
    (void)xUSBD_HID_Send_Report(mouse_ctx, (uint8_t *)mouse_report, sizeof(mouse_report));
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_HID_Mouse_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Mouse_App_Context_t *app_context)
{
    app_context->mouse_tx_complete = true;
    app_context->reset_complete = false;
    app_context->state = xUSBD_HID_MOUSE_APP_STATE_INIT;
    app_context->init_delay = 0U;
    app_context->mouse_send_delay = 0U;
    app_context->x_pos = 0;
    app_context->y_pos = 0;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_HID_Callbacks_t callbacks = {
        .on_bus_event = hid_mouse_app_bus_event,
        .on_transmit_complete = hid_mouse_app_transmit_complete,
        .on_get_report = NULL,
        .on_set_report = NULL,
        .on_data_received = NULL,
    };
    (void)xUSBD_HID_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_HID_Mouse_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_HID_Mouse_App_Context_t *app_context = hid_mouse_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return;
    }

    switch (app_context->state)
    {
    case xUSBD_HID_MOUSE_APP_STATE_INIT:
        if (app_context->reset_complete == true)
        {
            app_context->init_delay++;
            if (app_context->init_delay >= 0xFFFU)
            {
                app_context->reset_complete = false;
                app_context->state = xUSBD_HID_MOUSE_APP_STATE_SEND_DATA;
            }
        }
        break;

    case xUSBD_HID_MOUSE_APP_STATE_SEND_DATA:
        if (app_context->mouse_tx_complete == true)
        {
            app_context->mouse_send_delay++;
            if (app_context->mouse_send_delay >= 0xFFFU)
            {
                app_context->mouse_send_delay = 0U;
                app_context->mouse_tx_complete = false;
                hid_mouse_send_position(class_ctx, app_context->x_pos, app_context->y_pos);
                app_context->x_pos = (int8_t)(app_context->x_pos + 10);
                app_context->y_pos = (int8_t)(app_context->y_pos + 10);
                if (app_context->x_pos >= 100)
                {
                    app_context->x_pos = 10;
                    app_context->y_pos = 10;
                }
            }
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
