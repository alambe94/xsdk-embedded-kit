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

// @file xusbd_hid_keyboard_example.c
// @brief Application-level hooks and configuration for USB HID Keyboard.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_hid.h"
#include "xusbd_hid_keyboard_example.h"
#include "xusb_hid_keys.h"
#include "xusbd_return.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
const uint8_t g_xUSBD_Keyboard_Report_Descriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81,
    0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x03, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x03, 0x91, 0x02, 0x95, 0x05,
    0x75, 0x01, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x26, 0xff, 0x00, 0x05, 0x07, 0x19, 0x00, 0x29, 0x91, 0x81, 0x00, 0xC0};

const uint16_t g_xUSBD_Keyboard_Report_Descriptor_Len = (uint16_t)sizeof(g_xUSBD_Keyboard_Report_Descriptor);

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xUSBD_HID_Keyboard_App_Context_t *hid_keyboard_app_context_get(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t hid_keyboard_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t hid_keyboard_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xUSBD_HID_Keyboard_App_Context_t *hid_keyboard_app_context_get(xUSBD_Class_Context_t *class_ctx)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return NULL;
    }
    return (xUSBD_HID_Keyboard_App_Context_t *)app_context;
}

static xRETURN_t hid_keyboard_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_HID_Keyboard_App_Context_t *app_context = hid_keyboard_app_context_get(class_ctx);
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

static xRETURN_t hid_keyboard_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    xUSBD_HID_Keyboard_App_Context_t *app_context = hid_keyboard_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    app_context->keyboard_tx_complete = true;
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_HID_Keyboard_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Keyboard_App_Context_t *app_context)
{
    app_context->keyboard_tx_complete = true;
    app_context->reset_complete = false;
    app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_INIT;
    app_context->current_string = NULL;
    app_context->current_index = 0U;
    app_context->send_delay = 0U;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_HID_Callbacks_t callbacks = {
        .on_bus_event = hid_keyboard_app_bus_event,
        .on_transmit_complete = hid_keyboard_app_transmit_complete,
        .on_get_report = NULL,
        .on_set_report = NULL,
        .on_data_received = NULL,
    };
    (void)xUSBD_HID_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_HID_Keyboard_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    static const char s_message[] = "Hello HID keyboard!!!\r\n";

    xUSBD_HID_Keyboard_App_Context_t *app_context = hid_keyboard_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return;
    }

    switch (app_context->state)
    {
    case xUSBD_HID_KEYBOARD_APP_STATE_INIT:
        if (app_context->reset_complete == true)
        {
            app_context->send_delay++;
            if (app_context->send_delay >= 0xFFFU)
            {
                app_context->reset_complete = false;
                app_context->send_delay = 0U;
                app_context->current_string = s_message;
                app_context->current_index = 0U;
                app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_SEND_PRESS;
            }
        }
        break;

    case xUSBD_HID_KEYBOARD_APP_STATE_SEND_PRESS:
        if (app_context->keyboard_tx_complete == true)
        {
            uint8_t c = (uint8_t)app_context->current_string[app_context->current_index];
            if (c == 0U)
            {
                app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_IDLE;
                break;
            }
            if (c >= 32U && c <= 126U)
            {
                uint8_t keyboard_report[8] = {0U};
                uint8_t idx = c - 32U;
                keyboard_report[0] = ascii_to_hid_key_map[idx][0];
                keyboard_report[2] = ascii_to_hid_key_map[idx][1];
                app_context->keyboard_tx_complete = false;
                (void)xUSBD_HID_Send_Report(class_ctx, keyboard_report, sizeof(keyboard_report));
                app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_SEND_RELEASE;
            }
            else
            {
                app_context->current_index++;
            }
        }
        break;

    case xUSBD_HID_KEYBOARD_APP_STATE_SEND_RELEASE:
        if (app_context->keyboard_tx_complete == true)
        {
            uint8_t keyboard_report[8] = {0U};
            app_context->keyboard_tx_complete = false;
            (void)xUSBD_HID_Send_Report(class_ctx, keyboard_report, sizeof(keyboard_report));
            app_context->current_index++;
            app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_SEND_PRESS;
        }
        break;

    case xUSBD_HID_KEYBOARD_APP_STATE_IDLE:
        app_context->send_delay++;
        if (app_context->send_delay >= 0xFFFFU)
        {
            app_context->send_delay = 0U;
            app_context->current_index = 0U;
            app_context->current_string = s_message;
            app_context->state = xUSBD_HID_KEYBOARD_APP_STATE_SEND_PRESS;
        }
        break;

    default:
        break;
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
