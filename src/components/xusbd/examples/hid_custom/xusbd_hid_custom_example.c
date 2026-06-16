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

// @file xusbd_hid_custom_example.c
// @brief Application-level hooks and configuration for Custom USB HID.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include "string.h"

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusbd_hid.h"
#include "xusbd_hid_custom_example.h"
#include "xusbd_return.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////
const uint8_t g_xUSBD_Custom_Report_Descriptor[] = {
    0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,       // Usage (Vendor Usage 1)
    0xA1, 0x01,       // Collection (Application)
    // Input Report (from device to host)
    0x09, 0x02,       // Usage (Vendor Usage 2)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size (8 bits)
    0x95, 0x40,       // Report Count (64 bytes)
    0x81, 0x02,       // Input (Data, Variable, Absolute)
    // Output Report (from host to device)
    0x09, 0x03,       // Usage (Vendor Usage 3)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Size (8 bits)
    0x95, 0x40,       // Report Count (64 bytes)
    0x91, 0x02,       // Output (Data, Variable, Absolute)
    0xC0              // End Collection
};

const uint16_t g_xUSBD_Custom_Report_Descriptor_Len = (uint16_t)sizeof(g_xUSBD_Custom_Report_Descriptor);

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xUSBD_HID_Custom_App_Context_t *hid_custom_app_context_get(xUSBD_Class_Context_t *class_ctx);
static xRETURN_t hid_custom_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
static xRETURN_t hid_custom_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t hid_custom_app_get_report(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
static xRETURN_t hid_custom_app_set_report(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static xUSBD_HID_Custom_App_Context_t *hid_custom_app_context_get(xUSBD_Class_Context_t *class_ctx)
{
    void *app_context = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &app_context) != xRETURN_OK)
    {
        return NULL;
    }
    return (xUSBD_HID_Custom_App_Context_t *)app_context;
}

static xRETURN_t hid_custom_app_bus_event(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    xUSBD_HID_Custom_App_Context_t *app_context = hid_custom_app_context_get(class_ctx);
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

static xRETURN_t hid_custom_app_transmit_complete(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    xUSBD_HID_Custom_App_Context_t *app_context = hid_custom_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    app_context->custom_tx_complete = true;
    return xRETURN_OK;
}

static xRETURN_t hid_custom_app_get_report(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    xUSBD_HID_Custom_App_Context_t *app_context = hid_custom_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    response->data = app_context->loopback_buffer;
    response->length = app_context->loopback_len;
    return xRETURN_OK;
}

static xRETURN_t hid_custom_app_set_report(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length)
{
    xUSBD_HID_Custom_App_Context_t *app_context = hid_custom_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint32_t copy_len = (length > sizeof(app_context->loopback_buffer)) ? sizeof(app_context->loopback_buffer) : length;
    memcpy(app_context->loopback_buffer, control_data, copy_len);
    app_context->loopback_len = copy_len;
    app_context->loopback_pending = true;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
void xUSBD_HID_Custom_App_Init(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Custom_App_Context_t *app_context)
{
    app_context->custom_tx_complete = true;
    app_context->reset_complete = false;
    memset(app_context->loopback_buffer, 0, sizeof(app_context->loopback_buffer));
    app_context->loopback_len = 0U;
    app_context->loopback_pending = false;

    (void)xUSBD_Class_Set_App_Context(class_ctx, app_context);

    static xUSBD_HID_Callbacks_t callbacks = {
        .on_bus_event = hid_custom_app_bus_event,
        .on_transmit_complete = hid_custom_app_transmit_complete,
        .on_get_report = hid_custom_app_get_report,
        .on_set_report = hid_custom_app_set_report,
        .on_data_received = NULL,
    };
    (void)xUSBD_HID_Set_Callbacks(class_ctx, &callbacks);
}

void xUSBD_HID_Custom_App_Process(xUSBD_Class_Context_t *class_ctx)
{
    xUSBD_HID_Custom_App_Context_t *app_context = hid_custom_app_context_get(class_ctx);
    if (app_context == NULL)
    {
        return;
    }

    if (app_context->loopback_pending == true && app_context->custom_tx_complete == true)
    {
        app_context->loopback_pending = false;
        app_context->custom_tx_complete = false;
        (void)xUSBD_HID_Send_Report(class_ctx, app_context->loopback_buffer, app_context->loopback_len);
    }
}
// EOF /////////////////////////////////////////////////////////////////////////////
