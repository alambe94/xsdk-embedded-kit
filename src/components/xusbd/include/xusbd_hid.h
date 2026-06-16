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

// @file xusbd_hid.h
// @brief xUSB Human Interface Device (HID) class driver interface.

#ifndef XUSBD_HID_H
#define XUSBD_HID_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_class.h"
#include "xusb_hid_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBD_HID_DESC_BASE_SIZE   (USB_INTERFACE_DESC_LEN + USB_HID_DESC_LEN + USB_ENDPOINT_DESC_LEN)
#define xUSBD_HID_DESC_SIZE(speed) (xUSBD_HID_DESC_BASE_SIZE + (((speed) == USB_SPEED_SUPER) ? USB_SS_ENDPOINT_COMPANION_DESC_LEN : 0U))

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Generic HID Callbacks
    typedef struct xUSBD_HID_Callbacks_t
    {
        xRETURN_t (*on_bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
        xRETURN_t (*on_transmit_complete)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*on_get_report)(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
        xRETURN_t (*on_set_report)(xUSBD_Class_Context_t *class_ctx, uint8_t *control_data, uint32_t length);
        // Called when data arrives on an interrupt OUT endpoint. NULL for devices
        // that use only an IN endpoint (keyboard, mouse). Required for custom HID
        // devices that allocate an out_ep and need to receive host-to-device data
        // outside of EP0 SET_REPORT control transfers.
        xRETURN_t (*on_data_received)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    } xUSBD_HID_Callbacks_t;

    // Generic HID Context
    typedef struct xUSBD_HID_Context_t
    {
        xUSBD_Class_Context_t class_ctx;

        // Configuration parameters (filled by application before registration)
        const uint8_t *report_descriptor;
        uint16_t report_descriptor_len;
        uint8_t subclass; // Subclass: 0=None, 1=Boot
        uint8_t protocol; // Protocol: 0=None, 1=Keyboard, 2=Mouse
        uint8_t interval; // Polling interval in frames/ms

        // Allocated resources (filled by class allocators during init_instance)
        uint8_t interface;
        uint8_t in_ep;

        // Instance runtime state
        uint8_t hid_descriptor[USB_HID_DESC_LEN];
        uint8_t current_protocol;
        uint8_t idle_value;
        uint8_t alt_interface;
    } xUSBD_HID_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xUSBD_Class_Driver_t *xUSBD_HID_Class(void);
    xRETURN_t xUSBD_HID_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, xUSBD_HID_Callbacks_t *callbacks);
    xRETURN_t xUSBD_HID_Send_Report(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_HID_H
// EOF /////////////////////////////////////////////////////////////////////////////
